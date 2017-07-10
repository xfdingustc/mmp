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

#ifndef ANDROID_RMCLIENT_H_
#define ANDROID_RMCLIENT_H_

#include <binder/IInterface.h>
#include <binder/Parcel.h>

#include <media/stagefright/foundation/AHandlerReflector.h>
#include <utils/RefBase.h>
#include <utils/threads.h>

#include <utils/ValueUpdateListener_inl.h>

#include <RMClientListener.h>
#include <RMTypes.h>
#include <IRMClient.h>

namespace android {

class ALooper;
class AMessage;
class IRMService;
class RMClientListener;
class RMRequest;
class RMResponse;

class DebugId {
public:
    DebugId() : mId(getNewId()) {}

    int32_t get() {
        return mId;
    }

private:
    static int32_t getNewId() {
        AutoMutex l(mNextIdLock);
        return mNextId++;
    }

    int32_t mId;

    static int32_t mNextId;
    static Mutex mNextIdLock;
};


class RMClient : public BnRMClient,
                 public IBinder::DeathRecipient {
public:
    enum state_type {
        kStateLoaded = 10,
        kStateWaitingForResources,
        kStateIdle,
        kStateResourcePreempted,
    };

    enum {
        kWhatCmdResourceRequest = 100,
        kWhatCmdReturnAllResources,
    };

    explicit RMClient(const sp<RMClientListener> &listener,
                     RMClientPriority priority=kRMClientPriorityMedium);

    virtual void binderDied(const wp<android::IBinder> &binder);

    virtual void handleRequestDone(const sp<RMResponse> &resp);

    virtual void handleEvent(const sp<AMessage> &msg);

    virtual int32_t getId();

    virtual int32_t getState();

    void relayToListener(const sp<AMessage> &msg);

    // Used by AHandlerReflector
    void onMessageReceived(const sp<AMessage> &msg);

    void requestResourcesAsync(const sp<RMRequest> &request);

    bool returnAllResources();
    void returnAllResourcesAsync();

    // Call this when RM service is NULL or dead.
    bool refreshRM_l();

    // In testing, this will just return a stub RMService since mRMService was
    // not NULL from the beginning.
    sp<IRMService> getRM();

protected:
    virtual ~RMClient();  // only RefBase can call destructor

    sp<IRMService> mRMService;  // can be overriden in test

    DebugId mDebugId;

private:
    Mutex mRMServiceLock;

    sp<ALooper> mLooper;
    sp<AHandlerReflector<RMClient> > mReflector;
    sp<RMClientListener> mListener;
    RMClientPriority mPriority;
    sp<ValueUpdateListener<state_type> > mState;
};

}  // namespace android

#endif  // ANDROID_RMCLIENT_H_
