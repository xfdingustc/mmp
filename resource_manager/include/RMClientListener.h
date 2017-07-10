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

#ifndef ANDROID_RM_CLIENT_LISTENER_H_
#define ANDROID_RM_CLIENT_LISTENER_H_

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <utils/RefBase.h>

#include <RMTypes.h>

namespace android {

class AMessage;
class RMClient;
class RMRequest;
class RMResponse;

/**
 * A resource user should override this function to handle events sent from
 * RMService.
 */
class RMClientListener : public RefBase {
public:
    enum {
        // A resource request was processed.
        kWhatRequestDone = 10000,
        // One of allocated resources is preempted, and a prompt action is needed.
        kWhatResourcePreempted,
        // All the resources were returned back to resource manager.
        kWhatAllResourcesReturned,
    };

    RMClientListener();

    void setClient(const wp<RMClient> &client);

    ALooper::handler_id getHandlerId();

    // Used by AHandlerReflector
    void onMessageReceived(const sp<AMessage> &msg);

    virtual void handleRequestDone(const sp<RMResponse> &response) = 0;

    // Returns true if we want to return all the resources.
    // In case of temporary preemption, you may want to keep the resources,
    // and wait until resources become available, which isn't implemented yet.
    // Otherwise, you have to return resources immediately.
    virtual bool handleResourcePreempted() = 0;

    virtual void handleAllResourcesReturned() = 0;

    wp<RMClient> mClient;
    sp<ALooper> mLooper;
    sp<AHandlerReflector<RMClientListener> > mReflector;

protected:
    virtual ~RMClientListener();  // only RefBase can call destructor
};

}  // namespace android

#endif  // ANDROID_RM_CLIENT_LISTENER_H_
