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

#include <RMRequest.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

namespace android {

void RMRequest::addComponent(ComponentType type) {
    mComponents.add(type);
}

void RMRequest::addCapability(RMCapabilityType key, int32_t value) {
    mCapabilities.add(key, value);
}

const Vector<ComponentType> &RMRequest::getComponents() const {
    return mComponents;
}

const KeyedVector<RMCapabilityType, int32_t> &RMRequest::getCapabilities() const {
    return mCapabilities;
}

int32_t RMRequest::getCapability(RMCapabilityType type, int32_t defaultValue) const {
    ssize_t idx = mCapabilities.indexOfKey(type);
    if (idx < 0) {
        return defaultValue;
    }
    return mCapabilities.valueAt(idx);
}

void RMRequest::clear() {
    mComponents.clear();
    mCapabilities.clear();
}

void RMRequest::writeToParcel(Parcel *parcel) const {
    parcel->writeInt32(static_cast<int32_t>(mComponents.size()));
    for (size_t i = 0; i < mComponents.size(); ++i) {
        parcel->writeInt32(static_cast<int32_t>(mComponents[i]));
    }
    parcel->writeInt32(static_cast<int32_t>(mCapabilities.size()));
    for (size_t i = 0; i < mCapabilities.size(); ++i) {
        parcel->writeInt32(static_cast<int32_t>(mCapabilities.keyAt(i)));
        parcel->writeInt32(static_cast<int32_t>(mCapabilities.valueAt(i)));
    }
}

void RMRequest::readFromParcel(const Parcel &parcel) {
    clear();

    int32_t compSize = parcel.readInt32();
    for (int32_t i = 0; i < compSize; ++i) {
        mComponents.push(static_cast<ComponentType>(parcel.readInt32()));
    }

    int32_t capSize = parcel.readInt32();
    for (int32_t i = 0; i < capSize; ++i) {
        mCapabilities.add(static_cast<RMCapabilityType>(parcel.readInt32()),
                          parcel.readInt32());
    }
}

}  // namespace android
