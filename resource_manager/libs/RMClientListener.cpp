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
#define LOG_TAG "RMClientListener"
#include <utils/Log.h>

#include <IRMClient.h>
#include <RMClient.h>
#include <RMClientListener.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/RefBase.h>

#include <RMClient.h>
#include <RMResponse.h>

namespace android {

RMClientListener::RMClientListener()
    : mLooper(new ALooper()) {
    mLooper->setName("RMClientListener");

    // Virtually converts this class into AHandler.
    mReflector = new AHandlerReflector<RMClientListener>(this);
    mLooper->registerHandler(mReflector);
    CHECK_EQ(static_cast<status_t>(OK), mLooper->start(
            false,  /* runOnCallingThread */
            true,   /* canCallJava */
            PRIORITY_NORMAL));
}

RMClientListener::~RMClientListener() {
    ALOGV("~RMClientListener");
    mLooper->stop();
    mLooper->unregisterHandler(mReflector->id());
}

ALooper::handler_id RMClientListener::getHandlerId() {
    return mReflector->id();
}

void RMClientListener::setClient(const wp<RMClient> &client) {
    CHECK(mClient == NULL);  // you shouldn't call this function multiple times
    mClient = client;
}

void RMClientListener::onMessageReceived(const sp<AMessage> &msg) {
    switch(msg->what()) {
        case kWhatResourcePreempted: {
            ALOGV("kWhatResourcePreempted");
            bool returnResources = handleResourcePreempted();
            // We are not allowing resource pending for now.
            CHECK(returnResources);
            break;
        }
        default: {
            ALOGE("Error: msg %d not handled:", msg->what());
            break;
        }
    }
}

}  // namespace android
