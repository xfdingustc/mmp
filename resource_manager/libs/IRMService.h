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

#ifndef ANDROID_IRM_SERVICE_H_
#define ANDROID_IRM_SERVICE_H_

#include <sys/types.h>  // pid_t

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <media/stagefright/foundation/AHandlerReflector.h>

#include <utils/RefBase.h>
#include <utils/threads.h>

#include <RMTypes.h>

namespace android {

class ALooper;
class AMessage;
class IRMClient;
class RMAllocator;
class RMRequest;

class IRMService : public IInterface {
public:
    DECLARE_META_INTERFACE(RMService);

    virtual void requestResourcesAsync(const sp<IRMClient> &client,
                                       const sp<RMRequest> &request,
                                       RMClientPriority priority,
                                       pid_t pid) = 0;
    virtual void returnAllResources(const sp<IRMClient> &client) = 0;

    // Name under which this service is exported.
    static const String16 kServiceName;
};

class BpRMService : public BpInterface<IRMService> {
public:
    BpRMService(const sp<IBinder> &impl);

    virtual void requestResourcesAsync(const sp<IRMClient> &client,
                                       const sp<RMRequest> &request,
                                       RMClientPriority priority,
                                       pid_t pid);
    virtual void returnAllResources(const sp<IRMClient> &client);
};

class BnRMService : public BnInterface<IRMService> {
public:
    virtual status_t onTransact(uint32_t code,
                                const Parcel &data,
                                Parcel *reply,
                                uint32_t flags = 0);
};

}  // namespace android

#endif  // ANDROID_IRM_SERVICE_H_
