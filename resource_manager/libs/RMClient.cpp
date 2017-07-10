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

#define LOG_NDEBUG 0
#define LOG_TAG "RMClient"
#include <utils/Log.h>

#include <IRMClient.h>
#include <RMClient.h>

#include <unistd.h>  // getpid()

#include <binder/IServiceManager.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/String8.h>
#include <utils/threads.h>

#include <IRMService.h>
#include <RMClientListener.h>
#include <RMRequest.h>
#include <RMResponse.h>

namespace android {

static const int64_t kOneSecInUs = 1000000LL;

static AString parseEnum(int32_t msg) {
    return StringPrintf("%c%c%c%c",
                        (msg >> 24) & 0xff, (msg >> 16) & 0xff, (msg >> 8) & 0xff, msg & 0xff);
}


// --------------------------------------------------------
int32_t DebugId::mNextId = 1;
Mutex DebugId::mNextIdLock;

// --------------------------------------------------------
RMClient::RMClient(const sp<RMClientListener> &listener,
                   RMClientPriority priority)
    : mLooper(new ALooper()),
      mListener(listener),
      mPriority(priority),
      mState(new ValueUpdateListener<state_type>(kStateLoaded)) {
    ALOGV("#%d:RMClient", getId());
    mListener->setClient(this);
    mLooper->setName("RMClient");

    // Virtually converts this class into AHandler.
    mReflector = new AHandlerReflector<RMClient>(this);

    mLooper->registerHandler(mReflector);
    CHECK_EQ(static_cast<status_t>(OK), mLooper->start(
            false,  /* runOnCallingThread */
            true,   /* canCallJava */
            PRIORITY_NORMAL));
}

RMClient::~RMClient() {
    ALOGV("#%d:~RMClient", getId());
    returnAllResources();

    mLooper->stop();
    mLooper->unregisterHandler(mReflector->id());
}

void RMClient::binderDied(const wp<android::IBinder> &binder __unused) {
    ALOGE("#%d:binder died.", getId());
}

void RMClient::handleRequestDone(const sp<RMResponse> &resp) {
    ALOGV("#%d:handleRequestDone", getId());
    sp <AMessage> msg = new AMessage(RMClientListener::kWhatRequestDone,
                                     mReflector->id());
    msg->setObject("response", resp);
    msg->post();
}

void RMClient::handleEvent(const sp<AMessage> &msg) {
    ALOGV("#%d:handleEvent %s", getId(), parseEnum(msg->what()).c_str());
    msg->setTarget(mReflector->id());
    msg->post();
}

int32_t RMClient::getId() {
    return mDebugId.get();
}

int32_t RMClient::getState() {
    return mState->get();
}

void RMClient::relayToListener(const sp<AMessage> &msg) {
    ALOGV("#%d:Relaying messages to listener.", getId());
    CHECK(mListener != NULL);
    CHECK(msg != NULL);
    msg->setTarget(mListener->getHandlerId());
    msg->post();
}

void RMClient::onMessageReceived(const sp<AMessage> &msg) {
    ALOGV("#%d:onMessageReceived %s", getId(), parseEnum(msg->what()).c_str());
    switch(msg->what()) {
        case kWhatCmdResourceRequest: {
            ALOGV("#%d:kWhatCmdResourceRequest", getId());
            if (mState->get() == kStateResourcePreempted) {
                ALOGD("#%d is in kStateResourcePreempted state, do nothing", getId());
                return;
            }
            ALOGV("#%d:kWhatCmdResourceRequest with state:%d", getId(), mState->get());
            sp<AMessage> reply = new AMessage(RMClientListener::kWhatRequestDone,
                                             mListener->getHandlerId());

            sp<IRMService> rm = getRM();
            if (rm == NULL) {
                ALOGE("%d:No resource manager found.", getId());
                reply->setObject("response", NULL);
                reply->post();
                return;
            }

            sp<RefBase> obj;
            CHECK(msg->findObject("request", &obj));
            sp<RMRequest> request = static_cast<RMRequest *>(obj.get());
            mState->set(kStateWaitingForResources);
            rm->requestResourcesAsync(this, request, mPriority, getpid());
            break;
        }
        case kWhatCmdReturnAllResources: {
            ALOGV("#%d:kWhatCmdReturnAllResources", getId());
            sp<IRMService> rm = getRM();
            CHECK(rm != NULL);
            rm->returnAllResources(this);
            break;
        }
        case RMClientListener::kWhatRequestDone: {
            ALOGV("#%d:kWhatRequestDone", getId());
            if (mState->get() == kStateWaitingForResources) {
                sp<RefBase> obj;
                CHECK(msg->findObject("response", &obj));

                sp<RMResponse> response = static_cast<RMResponse *>(obj.get());
                mListener->handleRequestDone(response);
                mState->set(kStateIdle);
            } else {
                ALOGW("#%d:mState:%s, ignoring kWhatRequestDone", getId(),
                     parseEnum(mState->get()).c_str());
            }
            break;
        }
        case RMClientListener::kWhatResourcePreempted: {
            ALOGV("#%d:kWhatResourcePreempted", getId());
            if (mState->get() == kStateResourcePreempted) {
                break;  // ignore if preempted already
            }
            CHECK_EQ(kStateIdle, mState->get());
            mState->set(kStateResourcePreempted);
            relayToListener(msg);
            break;
        }
        case RMClientListener::kWhatAllResourcesReturned: {
            ALOGV("#%d:kWhatAllResourcesReturned", getId());
            mListener->handleAllResourcesReturned();
            mState->set(kStateLoaded);
            break;
        }
        default: {
            ALOGV("#%d:Unknown events.", getId());
            break;
        }
    }
    ALOGV("#%d:Done", getId());
}

void RMClient::requestResourcesAsync(const sp<RMRequest> &request) {
    ALOGV("#%d:requestResourceAsync", getId());
    sp<AMessage> msg = new AMessage(kWhatCmdResourceRequest, mReflector->id());
    msg->setObject("request", request);
    msg->post();
}

void RMClient::returnAllResourcesAsync() {
    ALOGV("#%d:RMClient::returnAllResourcesAsync", getId());
    sp<AMessage> msg = new AMessage(kWhatCmdReturnAllResources,
                                    mReflector->id());
    msg->post();
}

bool RMClient::returnAllResources() {
    returnAllResourcesAsync();
    // Wait for state change.
    return mState->waitForValue(kStateLoaded, kOneSecInUs);
}

bool RMClient::refreshRM_l() {
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        ALOGE("refreshRM_l:Failed to get default service manager.");
        return false;
    }
    sp<IBinder> binder = sm->getService(IRMService::kServiceName);
    if (binder == NULL) {
        ALOGE("refreshRM_l:Failed to get %s service.",
                String8(IRMService::kServiceName).string());
        return false;
    }
    mRMService = interface_cast<IRMService>(binder);
    return true;
}

sp<IRMService> RMClient::getRM() {
    AutoMutex l(mRMServiceLock);
    if (mRMService == NULL) {
        if (!refreshRM_l()) {
            return NULL;
        }
    }
    return mRMService;
}

}  // namespace android
