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

#ifndef ANDROID_IRMCLIENT_H_
#define ANDROID_IRMCLIENT_H_

#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {

class AMessage;
class RMResponse;

class IRMClient : public IInterface {
public:
    DECLARE_META_INTERFACE(RMClient);

    virtual void handleEvent(const sp<AMessage> &msg) = 0;
    virtual void handleRequestDone(const sp<RMResponse> &resp) = 0;

    virtual int32_t getId() = 0;
    virtual int32_t getState() = 0;
};

class BpRMClient : public BpInterface<IRMClient> {
public:
    explicit BpRMClient(const sp<IBinder> &impl);

    virtual void handleEvent(const sp<AMessage> &msg);
    virtual void handleRequestDone(const sp<RMResponse> &resp);

    virtual int32_t getId();
    virtual int32_t getState();
};

class BnRMClient : public BnInterface<IRMClient> {
public:
    virtual status_t onTransact(uint32_t code,
                                const Parcel &data,
                                Parcel *reply,
                                uint32_t flags = 0);
};

}  // namespace android

#endif  // ANDROID_IRMCLIENT_H_
