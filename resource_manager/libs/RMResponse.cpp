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
#define LOG_TAG "RMResponse"
#include <utils/Log.h>

#include <RMResponse.h>

#include <media/stagefright/foundation/ADebug.h>

namespace android {

Vector<Resource> &RMResponse::addComponent(ComponentType comp) {
    Vector<Resource> resVec;
    ssize_t idx = mResourceMap.add(comp, resVec);
    return mResourceMap.editValueAt(idx);
}

const CompResVec &RMResponse::getResourceMap() const {
    return mResourceMap;
}

CompResVec &RMResponse::editResourceMap() {
    return mResourceMap;
}

bool RMResponse::getFirstResourceFor(ComponentType comp, Resource *res) {
    ssize_t idx = mResourceMap.indexOfKey(comp);
    if (idx != NAME_NOT_FOUND && mResourceMap.valueAt(idx).size() > 0u) {
        *res = mResourceMap.valueAt(idx)[0];
        return true;
    }
    return false;
}

ssize_t RMResponse::getResourceNumber(ComponentType comp) {
    ssize_t pos = mResourceMap.indexOfKey(comp);
    if (pos != NAME_NOT_FOUND) {
        return mResourceMap.valueAt(pos).size();
    }
    return 0;
}

bool RMResponse::getResourceByIndex(ComponentType comp, Resource *res, const ssize_t idx) {
    ssize_t pos = mResourceMap.indexOfKey(comp);
    if (pos != NAME_NOT_FOUND && mResourceMap.valueAt(pos).size() > (unsigned)idx) {
        *res = mResourceMap.valueAt(pos)[idx];
        return true;
    }
    return false;
}

void RMResponse::clear() {
    mResourceMap.clear();
}

void RMResponse::writeToParcel(Parcel *parcel) const {
    uint32_t size = mResourceMap.size();
    parcel->writeInt32(static_cast<int32_t>(size));
    for (uint32_t i = 0; i < size; ++i) {
        parcel->writeInt32(static_cast<int32_t>(mResourceMap.keyAt(i)));

        const Vector<Resource> &resVec = mResourceMap.valueAt(i);
        parcel->writeInt32(static_cast<int32_t>(resVec.size()));
        for (uint32_t j = 0; j < resVec.size(); ++j) {
            parcel->writeInt32(static_cast<int32_t>(resVec[j]));
        }
    }
}

void RMResponse::readFromParcel(const Parcel &parcel) {
    clear();
    int32_t size = parcel.readInt32();
    for (int32_t i = 0; i < size; ++i) {
        ComponentType comp = static_cast<ComponentType>(parcel.readInt32());
        int32_t sizePerComp = parcel.readInt32();
        Vector<Resource> dummyResVec;
        ssize_t idx = mResourceMap.add(comp, dummyResVec);
        Vector<Resource> &resVec = mResourceMap.editValueAt(idx);

        for (int32_t j = 0; j < sizePerComp; ++j) {
            resVec.add(static_cast<Resource>(parcel.readInt32()));
        }
    }
}

}  // namespace android
