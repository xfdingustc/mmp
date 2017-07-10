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

#ifndef ANDROID_RM_REQUEST_H_
#define ANDROID_RM_REQUEST_H_

#include <binder/Parcel.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <utils/Vector.h>

#include <RMTypes.h>

namespace android {

class AMessage;

class RMRequest : public RefBase {
public:
    enum {
        kWhatResource = 'rres',
    };

    void addComponent(ComponentType type);
    void addCapability(RMCapabilityType, int32_t value);

    const Vector<ComponentType> &getComponents() const;
    const KeyedVector<RMCapabilityType, int32_t> &getCapabilities() const;
    int32_t getCapability(RMCapabilityType type, int32_t defaultValue) const;

    void clear();

    void writeToParcel(Parcel *parcel) const;
    void readFromParcel(const Parcel &parcel);

private:
    Vector<ComponentType> mComponents;
    KeyedVector<RMCapabilityType, int32_t> mCapabilities;
};

}  // namespace android

#endif  // ANDROID_RM_REQUEST_H_
