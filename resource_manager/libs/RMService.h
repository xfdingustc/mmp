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

#ifndef ANDROID_RM_SERVICE_H_
#define ANDROID_RM_SERVICE_H_

#include <utility>  // pair

#include <binder/IInterface.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include <IRMService.h>

namespace android {

class AMessage;
class ALooper;
class IRMClient;
class RMAllocator;

class RMService : public BnRMService {
public:
    // TODO: this is duplicate with RMClient.
    enum {
        kWhatCmdResourceRequest = 1000,
        kWhatCmdReturnAllResources,
        kWhatCmdWakeUpFromPreemptionPending,
    };

    // Only RefBase can be added to AMessage as an object, hence this wrapper.
    struct IRMClientWrapper : public RefBase {
        sp<IRMClient> client;
    };

    explicit RMService(const sp<RMAllocator> &resourceAllocator);

    virtual void requestResourcesAsync(const sp<IRMClient> &client,
                                       const sp<RMRequest> &request,
                                       RMClientPriority priority,
                                       pid_t pid);
    virtual void returnAllResources(const sp<IRMClient> &client);

    // Used by AHandlerReflector
    void onMessageReceived(const sp<AMessage> &msg);

    // IBinder interface
    virtual status_t dump(int fd, const Vector<String16>& args);
protected:
    virtual ~RMService();  // only RefBase can call destructor

private:
    void stopPreemptionPending();
    void removePendingResourceRequests(const sp<IRMClient> &from);
    void signalWakeUp(int64_t delayUs);

    sp<RMAllocator> mRMAllocator;
    sp<AHandlerReflector<RMService> > mReflector;
    sp<ALooper> mLooper;
    bool mPreemptionPending;
    List<sp<AMessage> > mPendingMessages;
};

}  // namespace android

#endif  // ANDROID_RM_SERVICE_H_
