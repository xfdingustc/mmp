/*
**
** Copyright 2012 The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "RMService"
#include <utils/Log.h>

#include <RMService.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>

#include <utils/RefBase.h>

#include <IRMClient.h>
#include <RMAllocator.h>
#include <RMClientListener.h>
#include <RMRequest.h>
#include <RMResponse.h>

namespace android {

static int getCallingPid() {
    return IPCThreadState::self()->getCallingPid();
}

static int getCallingUid() {
    return IPCThreadState::self()->getCallingUid();
}

// --------------------------------------------------------

RMService::RMService(const sp<RMAllocator> &resourceAllocator)
    : mRMAllocator(resourceAllocator),
      mPreemptionPending(false) {
    ALOGV("RMService constructor");
    mLooper = new ALooper();
    mLooper->setName("RMService");

    // Virtually converts this class into AHandler.
    mReflector = new AHandlerReflector<RMService>(this);
    mLooper->registerHandler(mReflector);
    mLooper->start(
            false,  /* runOnCallingThread */
            true,   /* canCallJava */
            PRIORITY_AUDIO);
}

RMService::~RMService() {
    ALOGV("~RMService");
    mLooper->stop();
    mLooper->unregisterHandler(mReflector->id());
}

void RMService::requestResourcesAsync(const sp<IRMClient> &client,
                                     const sp<RMRequest> &request,
                                     RMClientPriority priority,
                                     pid_t pid) {
    ALOGV("RMService::requestResourcesAsync: %p", client.get());
    sp<AMessage> msg = new AMessage(kWhatCmdResourceRequest, mReflector->id());
    // Note: we cannot use setObject here since findObject requires the value
    // to be converted to RefBase, but IRMClient is derived from IInterface
    // which in turn is a virtual subclass of RefBase.

    sp<IRMClientWrapper> wrapper = new IRMClientWrapper;
    wrapper->client = client;
    msg->setObject("client", wrapper);
    msg->setObject("request", request);
    msg->setInt32("priority", static_cast<int32_t>(priority));
    msg->setInt32("pid", static_cast<int32_t>(pid));
    msg->post();
}

void RMService::returnAllResources(const sp<IRMClient> &client) {
    ALOGV("RMService::returnAllResources for #%d", client->getId());
    sp<AMessage> msg = new AMessage(kWhatCmdReturnAllResources,
                                    mReflector->id());
    sp<IRMClientWrapper> wrapper = new IRMClientWrapper;
    wrapper->client = client;
    msg->setObject("client", wrapper);
    msg->post();
}

void RMService::stopPreemptionPending() {
    mPreemptionPending = false;
    for (List<sp<AMessage> >::iterator it = mPendingMessages.begin();
            it != mPendingMessages.end();
            ++it) {
        (*it)->post();
    }
    mPendingMessages.clear();
}

void RMService::removePendingResourceRequests(const sp<IRMClient> &from) {
    CHECK(from != NULL);
    List<sp<AMessage> >::iterator it = mPendingMessages.begin();
    while (it != mPendingMessages.end()) {
        const sp<AMessage> &msg = *it;
        if (msg->what() == kWhatCmdResourceRequest) {
            sp<RefBase> obj;
            CHECK(msg->findObject("client", &obj));
            sp<IRMClientWrapper> wrapper = static_cast<IRMClientWrapper *>(
                    obj.get());
            CHECK(wrapper != NULL);
            sp<IRMClient> client = wrapper->client;
            if (client->asBinder() == from->asBinder()) {
                it = mPendingMessages.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void RMService::signalWakeUp(int64_t delayUs) {
    sp<AMessage> wakeUpMsg = new AMessage(kWhatCmdWakeUpFromPreemptionPending,
                                          mReflector->id());
    wakeUpMsg->post(delayUs);
}

void RMService::onMessageReceived(const sp<AMessage> &msg) {
    ALOGV("onMessageReceived");
    const int64_t kDelayUs = 500000;  // 0.5 secs
    switch(msg->what()) {
        case kWhatCmdResourceRequest: {
            ALOGV("kWhatCmdResourceRequest");
            if (mPreemptionPending) {
                ALOGV("Pending another request message");
                mPendingMessages.push_back(msg);
                signalWakeUp(kDelayUs);
                break;
            }
            sp<RefBase> obj1;
            CHECK(msg->findObject("client", &obj1));
            sp<IRMClientWrapper> wrapper = static_cast<IRMClientWrapper *>(obj1.get());
            sp<IRMClient> client = wrapper->client;

            sp<RefBase> obj2;
            CHECK(msg->findObject("request", &obj2));
            sp<RMRequest> request = static_cast<RMRequest *>(obj2.get());

            RMClientPriority priority;
            int32_t int1;
            CHECK(msg->findInt32("priority", &int1));
            priority = static_cast<RMClientPriority>(int1);

            pid_t pid;
            int32_t int2;
            CHECK(msg->findInt32("pid", &int2));
            pid = static_cast<pid_t>(int2);

            bool retry = false;
            sp<RMResponse> response =
                    mRMAllocator->handleRequest(client, request, priority, pid, &retry);

            if (response == NULL && retry) {
                ALOGV("Pending until preemption is done.");
                mPreemptionPending = true;
                mPendingMessages.push_back(msg);
                signalWakeUp(kDelayUs);
                break;
            }

            client->handleRequestDone(response);
            break;
        }

        case kWhatCmdReturnAllResources: {
            ALOGV("kWhatCmdReturnAllResources");
            sp<RefBase> obj1;
            CHECK(msg->findObject("client", &obj1));
            sp<IRMClientWrapper> wrapper = static_cast<IRMClientWrapper *>(obj1.get());
            sp<IRMClient> client = wrapper->client;

            mRMAllocator->returnAllResourcesFor(client);
            sp<AMessage> reply;
            reply = new AMessage(RMClientListener::kWhatAllResourcesReturned);
            reply->setInt32("result", 1);
            client->handleEvent(reply);

            removePendingResourceRequests(client);
            stopPreemptionPending();
            break;
        }

        case kWhatCmdWakeUpFromPreemptionPending: {
            ALOGV("kWhatCmdWakeUpFromPreemptionPending");
            stopPreemptionPending();
            break;
        }

        default: {
            ALOGE("not recognized message: %d", msg->what());
        }
    }
}

status_t RMService::dump(int fd, const Vector<String16>& args __unused) {
    String8 result;
    static const String16 sDump("android.permission.DUMP");
    if (!PermissionCache::checkCallingPermission(sDump)) {
        result.appendFormat("Permission Denial: "
                "can't dump RMService from pid=%d, uid=%d\n",
                getCallingPid(),
                getCallingUid());
        write(fd, result.string(), result.size());
    } else {
        result.appendFormat("Dumping resource manager\n");
        result.appendFormat("  mPreemptionPending is %d\n", mPreemptionPending);
        write(fd, result.string(), result.size());
        mRMAllocator->dump(fd);
    }
    return NO_ERROR;
}

}  // namespace android
