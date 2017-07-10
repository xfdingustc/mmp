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
#define LOG_TAG "IRMService"
#include <utils/Log.h>

#include <IRMService.h>

#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>

#include <utils/RefBase.h>
#include <utils/threads.h>

#include <IRMClient.h>
#include <RMRequest.h>

namespace android {

enum {
    REQUEST_RESOURCES_ASYNC = IBinder::FIRST_CALL_TRANSACTION,
    RETURN_ALL_RESOURCES,
};

const String16 IRMService::kServiceName("media.hw_resourcemanager");

BpRMService::BpRMService(const sp<IBinder> &impl)
    : BpInterface<IRMService>(impl) {
}

void BpRMService::requestResourcesAsync(const sp<IRMClient> &client,
                                        const sp<RMRequest> &request,
                                        RMClientPriority priority,
                                        pid_t pid) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IRMService::getInterfaceDescriptor());
    data.writeStrongBinder(client->asBinder());
    request->writeToParcel(&data);
    data.writeInt32(priority);
    data.writeInt32(pid);
    remote()->transact(REQUEST_RESOURCES_ASYNC, data, &reply);
}

void BpRMService::returnAllResources(const sp<IRMClient> &client) {
    ALOGV("BpRMService::returnAllResources");
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IRMService::getInterfaceDescriptor());
    data.writeStrongBinder(client->asBinder());
    remote()->transact(RETURN_ALL_RESOURCES, data, &reply);
}

IMPLEMENT_META_INTERFACE(RMService, "android.rmservice");

// -------------------------------------------------------------------

status_t BnRMService::onTransact(uint32_t code,
                                 const Parcel &data,
                                 Parcel *reply,
                                 uint32_t flags) {
    ALOGV("BnRMService::onTransact");
    switch(code) {
        case REQUEST_RESOURCES_ASYNC: {
            CHECK_INTERFACE(IRMService, data, reply);
            sp<IRMClient> client = interface_cast<IRMClient>(
                    data.readStrongBinder());
            sp<RMRequest> request = new RMRequest();
            request->readFromParcel(data);
            RMClientPriority priority = static_cast<RMClientPriority>(
                    data.readInt32());
            pid_t pid = static_cast<pid_t>(data.readInt32());
            requestResourcesAsync(client, request, priority, pid);
            break;
        }
        case RETURN_ALL_RESOURCES: {
            CHECK_INTERFACE(IRMService, data, reply);
            sp<IRMClient> client = interface_cast<IRMClient>(
                    data.readStrongBinder());
            returnAllResources(client);
            break;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
    ALOGV("BnRMService::onTransact done");
    return OK;
}

}  // namespace android
