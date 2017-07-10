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

#ifndef ANDROID_RM_TYPES_H_
#define ANDROID_RM_TYPES_H_

namespace android {

typedef uint32_t Resource;
typedef uint32_t ResourceType;

// Defines component types. Resource users will request resources by each component type.
enum ComponentType {
    kComponentVideoRenderer = 10,
    kComponentAVINMaster,
};

// Defines capability types
enum RMCapabilityType {
    kRMCapabilityVideoDeInterlace = 1000,
    kRMCapabilityVideoPIP,
};

// Defines priorities for resource manager client
enum RMClientPriority {
    kRMClientPriorityMin = 2000,
    kRMClientPriorityLow,
    kRMClientPriorityMedium,
    kRMClientPriorityHigh,
    kRMClientPriorityMax,
};

}  // namespace android

#endif  // ANDROID_RM_TYPES_H_
