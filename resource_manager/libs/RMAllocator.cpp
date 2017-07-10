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

// TODO: Comment out the below line once resource manager becomes stable.
#define LOG_NDEBUG 0
#define LOG_TAG "RMAllocator"
#include <utils/Log.h>

#include <RMAllocator.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/RefBase.h>
#include <utils/String8.h>

#include <IRMClient.h>
#include <RMClientListener.h>
#include <RMRequest.h>
#include <RMResponse.h>

namespace android {

RMAllocator::RMAllocator() {
    ALOGV("RMAllocator constructor");
    addComponentType(kComponentVideoRenderer);
    addComponentType(kComponentAVINMaster);
}

RMAllocator::~RMAllocator() {
    ALOGV("RMAllocator destructor");
}

void RMAllocator::addComponentType(ComponentType compType) {
    Vector<ResourceType> v;
    mCompResourceTypeMap.add(compType, v);
}

void RMAllocator::addResourceType(ComponentType compType,
                                  ResourceType resourceType) {
    Vector<ResourceType> &v = mCompResourceTypeMap.editValueFor(compType);
    v.push(resourceType);
    SortedVector<Resource> s;
    mTypeResourceMap.add(resourceType, s);
}

void RMAllocator::addResource(ResourceType resourceType, Resource resource) {
    ssize_t i = mTypeResourceMap.indexOfKey(resourceType);
    CHECK_GE(i, 0);
    SortedVector<Resource> &s = mTypeResourceMap.editValueAt(i);
    s.add(resource);
}

void RMAllocator::addResource(ResourceType resourceType, Resource resource,
                              int32_t vendorResource) {
    ssize_t i = mTypeResourceMap.indexOfKey(resourceType);
    CHECK_GE(i, 0);
    SortedVector<Resource> &s = mTypeResourceMap.editValueAt(i);
    s.add(resource);
    mVendorResourceMap.add(resource, vendorResource);
}

void RMAllocator::dump(int fd) {
    String8 result;
    result.appendFormat("  clients count: %d\n", mClientMap.size());
    result.appendFormat("  resources states:\n");
    for (size_t i = 0u; i < mTypeResourceMap.size(); ++i) {
        const SortedVector<Resource> &s = mTypeResourceMap.valueAt(i);
        for (size_t j = 0u; j < s.size(); ++j) {
            const Resource &r = s[j];
            if (mResourceClientMap.valueFor(r) == NULL) {
                result.appendFormat("     resource %d is free\n", static_cast<int>(r));
            } else {
                result.appendFormat("     resource %d is allocated by #%d,%p with state %d\n",
                                   static_cast<int>(r),
                                   mResourceClientMap.valueFor(r)->id(),
                                   mResourceClientMap.valueFor(r)->getRMClient()->asBinder().get(),
                                   mResourceClientMap.valueFor(r)->getRMClient()->getState());
            }
        }
    }
    write(fd, result.string(), result.size());
}

sp<RMResponse> RMAllocator::handleRequest(const sp<IRMClient> &client,
                                         const sp<RMRequest> &request,
                                         RMClientPriority priority,
                                         pid_t pid,
                                         bool *retry) {
    ALOGV("handleRequest");

    // Make client info and add it to mClientMap
    bool client_in_map = false;
    sp<ClientInfo> clientInfo = new ClientInfo(this, client, priority, pid);
    sp<IBinder> binder = client->asBinder();
    binder->linkToDeath(clientInfo);
    ssize_t idx = mClientMap.indexOfKey(client->asBinder());
    if (idx >= 0) {
        ALOGV("Client #%d is already in mClientMap", client->getId());
        client_in_map = true;
    } else {
        mClientMap.replaceValueFor(binder, clientInfo);
    }
    CHECK_GE(mClientMap.size(), 0u);
    // Allocate resources
    Vector<TCMVector> resourceMap;
    if (!generateResourceMapWithPower(request, &resourceMap)) {
        if (!client_in_map) {
            ssize_t idx = mClientMap.indexOfKey(client->asBinder());
            mClientMap.removeItemsAt(idx);
        }
        return NULL;
    }
    Vector<sp<ClientInfo> > clientsMap;
    if (decideClientsToPreemptFrom(client, request, resourceMap, &clientsMap)) {
        size_t size = clientsMap.size();
        if (size > 0) {
            for (size_t i = 0u; i < size; ++i) {
                preemptResourcesFrom(clientsMap[i]);
                *retry = true;
            }
            return NULL;
        }
    }

    sp<RMResponse> response = allocatePowerResources(client, request, resourceMap);
    if (response == NULL && !client_in_map) {
        returnAllResourcesFor(client);
    }

    return response;
}

void RMAllocator::returnAllResourcesFor(const sp<IRMClient> &client) {
    if (client == NULL) {
        ALOGW("returnAllResourcesFor: NULL");
        return;
    }
    disownResources(mClientMap.valueFor(client->asBinder()));
    ssize_t idx = mClientMap.indexOfKey(client->asBinder());
    if (idx < 0) {
        ALOGW("returnAllResourcesFor: not registered, nothing to do");
    } else {
        mClientMap.removeItemsAt(idx);
    }
}

const Vector<ResourceType> &RMAllocator::getResourceTypes(ComponentType compType) {
    return mCompResourceTypeMap.valueFor(compType);
}

bool RMAllocator::getResourcesForType(ResourceType resourceType,
                                     const SortedVector<Resource> **resources) {
    ssize_t i = mTypeResourceMap.indexOfKey(resourceType);
    if (i < 0) {
        return false;
    }
    *resources = &(mTypeResourceMap.valueAt(i));
    return true;
}

void RMAllocator::preemptResourcesFrom(const sp<ClientInfo> &from) {
    ALOGI("Preempting resources from client #%d", getClientId(from));

    sp<AMessage> msg = new AMessage(RMClientListener::kWhatResourcePreempted);
    from->getRMClient()->handleEvent(msg);
}

int32_t RMAllocator::getClientId(const sp<ClientInfo> &client) {
    if (client == NULL) {
        return 0;
    }
    return client->id();
}

void RMAllocator::disownResources(const sp<ClientInfo> &owner) {
    if (owner == NULL) {
        ALOGW("owner does not exist while trying to disown resources.");
        return;
    }
    ssize_t i = mClientMap.indexOfKey(owner->getRMClient()->asBinder());
    if (i >= 0) {
        ALOGV("Deallocating resources for #%d", getClientId(owner));
        const SortedVector<std::pair<ComponentType, Resource> > &s = owner->getComponentResources();
        for (size_t j = 0u; j < s.size(); ++j) {
            const ComponentType &c = s[j].first;
            const Resource &r = s[j].second;
            mResourceClientMap.removeItem(r);
            ALOGV(" return #%d", static_cast<int>(r));
            onResourceReturned(c, r);
        }
        mClientMap.removeItemsAt(i);
    }
}


int32_t RMAllocator::translate(Resource resource) {
    ssize_t v = mVendorResourceMap.indexOfKey(resource);
    if (v >= 0) {
        return mVendorResourceMap.valueAt(v);
    } else {
        return resource;
    }
}

bool RMAllocator::generateResourceMapWithPower(const sp<RMRequest> &request,
                                              Vector<TCMVector> *resourceMap) {
    const Vector<ComponentType> &comps = request->getComponents();
    for (size_t i = 0u; i < comps.size(); ++i) {
        const Vector<ResourceType> &resourceTypes = getResourceTypes(comps[i]);
        {
            TCMVector v;
            resourceMap->push(v);
        }
        TCMVector &tcmVector = resourceMap->editTop();

        for (size_t i = 0u; i < resourceTypes.size(); ++i) {
            Vector<Resource> candidates;
            int32_t power;
            if (!getCandidatesWithPower(resourceTypes[i], request, &candidates, &power)) {
                ALOGW("getCandidates returned false for resource type: %d",
                        resourceTypes[i]);
                return false;
            }
            sp<TypeCandidatesMapWithPower> tcm = new TypeCandidatesMapWithPower();
            tcm->mType = resourceTypes[i];
            tcm->mPower = power;
            for (size_t j = 0u; j < candidates.size(); ++j) {
                tcm->mResourceClientList.push(std::make_pair(
                        candidates[j], mResourceClientMap.valueFor(candidates[j])));
            }
            tcmVector.push(tcm);
        }
    }
    return true;
}

bool RMAllocator::decideClientsToPreemptFrom(const sp<IRMClient> &client,
                                            const sp<RMRequest> &request,
                                            const Vector<TCMVector> &resourceMap,
                                            Vector<sp<ClientInfo> > *clientsMap) {
    CHECK_EQ(request->getComponents().size(), resourceMap.size());

    for (size_t i = 0u; i < resourceMap.size(); ++i) {
        for (size_t j = 0u; j < resourceMap[i].size(); ++j) {
            if (resourceMap[i][j]->mResourceClientList.empty()) {
                // Skips when there is no resource candidate.
                continue;
            }

            if (decideClientsToPreemptFromByComponent(client, resourceMap[i][j], clientsMap)) {
                if (clientsMap->size() > 0) {
                    return true;
                } else {
                    ALOGV("Go to next");
                }
            }
        }
    }
    return false;
}

bool RMAllocator::decideClientsToPreemptFromByComponent(const sp<IRMClient> &client,
                                                       const sp<TypeCandidatesMapWithPower> &tcm,
                                                       Vector<sp<ClientInfo> > *clientsMap) {
    const sp<ClientInfo> &newOwner = mClientMap.valueFor(client->asBinder());
    CHECK(newOwner != NULL);

    int power_candidates = 0;
    // First pass: look for resource assigned to none
    for (size_t i = 0u; i < tcm->mResourceClientList.size(); ++i) {
        const sp<ClientInfo> &currentOwner = tcm->mResourceClientList[i].second;

        // Re-evaluate since client may forget to return resources until now.
        if (currentOwner == NULL) {
            ALOGV("Res %d: No owner", tcm->mResourceClientList[i].first);
            power_candidates++;
            if (power_candidates >= tcm->mPower) {
                return true;
            }
        }
    }

    // Second pass: look for resource assigned to a lower or same
    // priority client.
    for (size_t i = 0u; i < tcm->mResourceClientList.size(); ++i) {
        const sp<ClientInfo> &currentOwner = tcm->mResourceClientList[i].second;
        if (currentOwner != NULL) {
        	if (currentOwner->getRMClient()->asBinder() == client->asBinder()) {
        	    ALOGV("Same client");
        	} else if (currentOwner->getPriority() <= newOwner->getPriority()) {
                ALOGV("Res %d: Higer priority", tcm->mResourceClientList[i].first);
                power_candidates++;
                addClientIfNecessary(clientsMap, currentOwner);
                if (power_candidates >= tcm->mPower) {
                    return true;
                }
            } else {
                ALOGV("Res %d: Lower priority", tcm->mResourceClientList[i].first);
            }
        }
    }

    clientsMap->clear();
    return false;
}

void RMAllocator::addClientIfNecessary(Vector<sp<ClientInfo> > *clientsMap,
                                      const sp<ClientInfo> &client) {
    size_t j = 0u;
    for (; j < clientsMap->size(); ++j) {
        if (clientsMap->itemAt(j) == client) {
            ALOGV("Owners are same");
            break;
        }
    }
    if (j >= clientsMap->size()) {
        clientsMap->push(client);
    }
}

sp<RMResponse> RMAllocator::allocatePowerResources(const sp<IRMClient> &client,
                                                  const sp<RMRequest> &request,
                                                  const Vector<TCMVector> &resourceMap) {
    ALOGV("allocatePowerResources");

    CHECK_EQ(request->getComponents().size(), resourceMap.size());

    sp<RMResponse> response = new RMResponse();
    for (size_t i = 0u; i < resourceMap.size(); ++i) {
        ComponentType comp = request->getComponents()[i];
        Vector<Resource> &resourcesPerComp = response->addComponent(comp);
        ALOGV("reserve component: %d", comp);

        for (size_t j = 0u; j < resourceMap[i].size(); ++j) {
            if (resourceMap[i][j]->mResourceClientList.empty()) {
                // Skips when there is no resource candidate.
                continue;
            }

            Vector<Resource> resources;
            if (allocateOnePowerResource(client, resourceMap[i][j], comp, resources)) {
                for (size_t t = 0u; t < resources.size(); ++t) {
                    onResourceReserved(comp, request, resources[t]);
                    resourcesPerComp.add(translate(resources[t]));
                }
            } else {
                ALOGW("Failed to allocate a resource for component: %d", comp);
                return NULL;
            }
        }
    }
    return response;
}

bool RMAllocator::allocateOnePowerResource(const sp<IRMClient> &client,
                                          const sp<TypeCandidatesMapWithPower> &tcm,
                                          const ComponentType &comp,
                                          Vector<Resource> &resources) {
    ALOGV("allocateOnePowerResource");
    const sp<ClientInfo> &newOwner = mClientMap.valueFor(client->asBinder());
    CHECK(newOwner != NULL);

    // Look for resource assigned to none
    int32_t allocated_power = 0;
    for (size_t i = 0u; i < tcm->mResourceClientList.size(); ++i) {
        const sp<ClientInfo> &currentOwner = tcm->mResourceClientList[i].second;

        // Re-evaluate since client may forget to return resources until now.
        if (currentOwner == NULL) {
            Resource resource = tcm->mResourceClientList[i].first;
            resources.add(resource);

            // Updates mResourceClientMap
            mResourceClientMap.replaceValueFor(resource, newOwner);
            CHECK(newOwner == mResourceClientMap.valueFor(resource));

            newOwner->addComponentResource(comp, resource);
            allocated_power++;
            if (allocated_power >= tcm->mPower) {
                return true;
            }
        }
    }
    ALOGV("allocated_power %d", allocated_power);
    CHECK(allocated_power == 0);
    return false;
}

}  // namespace android
