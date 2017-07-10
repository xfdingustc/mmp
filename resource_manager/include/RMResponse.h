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

#ifndef ANDROID_RM_RESPONSE_H_
#define ANDROID_RM_RESPONSE_H_

#include <binder/Parcel.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <utils/Vector.h>

#include <RMTypes.h>

namespace android {

typedef KeyedVector<ComponentType, Vector<Resource> > CompResVec;

class RMResponse : public RefBase {
public:
    Vector<Resource> &addComponent(ComponentType comp);
    const CompResVec &getResourceMap() const;
    CompResVec &editResourceMap();
    bool getFirstResourceFor(ComponentType comp, Resource *res);

    void clear();

    void writeToParcel(Parcel *parcel) const;
    void readFromParcel(const Parcel &parcel);

    ssize_t getResourceNumber(ComponentType comp);
    bool getResourceByIndex(ComponentType comp, Resource *res, const ssize_t idx);

private:
    CompResVec mResourceMap;
};

}  // namespace android

#endif  // ANDROID_RM_RESPONSE_H_
