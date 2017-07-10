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
#define LOG_TAG "IRMClient"
#include <utils/Log.h>

#include <IRMClient.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

#include <RMResponse.h>

namespace android {

enum {
    HANDLE_EVENT = IBinder::FIRST_CALL_TRANSACTION,
    HANDLE_REQUEST_DONE,
    GET_ID,
    GET_STATE,
};

BpRMClient::BpRMClient(const sp<IBinder> &impl)
    : BpInterface<IRMClient>(impl) {
}

void BpRMClient::handleEvent(const sp<AMessage> &msg) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IRMClient::getInterfaceDescriptor());
    msg->writeToParcel(&data);
    remote()->transact(HANDLE_EVENT, data, &reply);
}

void BpRMClient::handleRequestDone(const sp<RMResponse> &resp) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IRMClient::getInterfaceDescriptor());
    if (resp != NULL) {
        resp->writeToParcel(&data);
    }
    remote()->transact(HANDLE_REQUEST_DONE, data, &reply);
}

int32_t BpRMClient::getId() {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IRMClient::getInterfaceDescriptor());
    remote()->transact(GET_ID, data, &reply);
    return reply.readInt32();
}

int32_t BpRMClient::getState() {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IRMClient::getInterfaceDescriptor());
    remote()->transact(GET_STATE, data, &reply);
    return reply.readInt32();
}

IMPLEMENT_META_INTERFACE(RMClient, "android.rmclient");

// -------------------------------------------------------------------

status_t BnRMClient::onTransact(uint32_t code,
                                const Parcel &data,
                                Parcel *reply,
                                uint32_t flags) {
    CHECK_INTERFACE(IRMClient, data, reply);
    switch(code) {
        case HANDLE_EVENT: {
            ALOGV("HANDLE_EVENT");
            sp<AMessage> msg = AMessage::FromParcel(data);
            handleEvent(msg);
            break;
        }
        case HANDLE_REQUEST_DONE: {
            ALOGV("HANDLE_REQUEST_DONE");
            if (data.dataAvail() > 0) {
                sp<RMResponse> resp = new RMResponse();
                resp->readFromParcel(data);
                handleRequestDone(resp);
            } else {
                handleRequestDone(NULL);
            }
            break;
        }
        case GET_ID: {
            ALOGV("GET_ID");
            int32_t id = getId();
            reply->writeInt32(id);
            break;
        }
        case GET_STATE: {
            ALOGV("GET_STATE");
            int32_t state = getState();
            reply->writeInt32(state);
            break;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
    return OK;
}

}  // namespace android
