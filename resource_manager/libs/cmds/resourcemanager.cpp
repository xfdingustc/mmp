/*
**
** Copyright 2012, The Android Open Source Project
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

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <MVBerlinRMAllocator.h>
#include <RMService.h>

using namespace android;

int main(int argc __unused, char** argv __unused) {
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        ALOGE("failed to be added to ServiceManager, crash ourself");
        exit(1);
    }

    status_t result = sm->addService(IRMService::kServiceName,
                                    new RMService(new MVBerlinRMAllocator()));
    if (result != OK) {
        ALOGE("failed to be added to ServiceManager, exit ourself");
        exit(1);
    }

    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();

    return 0;
}
