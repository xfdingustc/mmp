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

#ifndef ANDROID_RM_ALLOCATOR_H_
#define ANDROID_RM_ALLOCATOR_H_

#include <sys/types.h>  // pid_t

#include <utility>  // pair

#include <binder/IBinder.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/SortedVector.h>

#include <IRMClient.h>
#include <RMTypes.h>

namespace android {

class RMRequest;
class RMResponse;


class RMAllocator : public RefBase {
public:
    RMAllocator();

    virtual sp<RMResponse> handleRequest(const sp<IRMClient> &client,
                                         const sp<RMRequest> &request,
                                         RMClientPriority priority,
                                         pid_t pid,
                                         bool *retry);

    virtual void returnAllResourcesFor(const sp<IRMClient> &client);
    
    virtual void dump(int fd);

protected:
    virtual ~RMAllocator();  // only RefBase can call destructor

    class ClientInfo : public IBinder::DeathRecipient {
    public:
        ClientInfo(const sp<RMAllocator> &rmAllocator,
                   const sp<IRMClient> &rmClient,
                   RMClientPriority priority,
                   pid_t pid)
            : mRMAllocator(rmAllocator),
              mRMClient(rmClient),
              mPriority(priority),
              mPid(pid) {
        }

        virtual ~ClientInfo() {
            ALOGV("~ClientInfo");
        }

        // IBinder::DeathRecipient
        virtual void binderDied(const wp<IBinder> &who __unused) {
            ALOGI("A client died.");
            // TODO: there needs a lock
            sp<ClientInfo> keep(this);
            mRMAllocator->returnAllResourcesFor(mRMClient);
        }

        int32_t id() {
            if (mRMClient == NULL) {
                return 0;
            }
            return mRMClient->getId();
        }

        void addComponentResource(const ComponentType &comp, const Resource &res) {
            mCompResources.add(std::make_pair(comp, res));
        }

        const SortedVector<std::pair<ComponentType, Resource> > &getComponentResources() {
            return mCompResources;
        }

        RMClientPriority getPriority() {
            return mPriority;
        }

        sp<IRMClient> getRMClient() {
            return mRMClient;
        }

    private:
        sp<RMAllocator> mRMAllocator;
        sp<IRMClient> mRMClient;
        RMClientPriority mPriority;
        pid_t mPid;
        SortedVector<std::pair<ComponentType, Resource> > mCompResources;
    };

    void addComponentType(ComponentType compType);
    void addResourceType(ComponentType compType, ResourceType resourceType);
    void addResource(ResourceType resourceType, Resource resource);
    void addResource(ResourceType resourceType, Resource resource,
                     int32_t vendorResource);

    const Vector<ResourceType> &getResourceTypes(ComponentType compType);
    bool getResourcesForType(ResourceType resourceType,
                            const SortedVector<Resource> **resources);
    sp<ClientInfo> getClientForResource(Resource resource);


    void preemptResourcesFrom(const sp<ClientInfo> &from);

    int32_t getClientId(const sp<ClientInfo> &client);  // for debugging

    void disownResources(const sp<ClientInfo> &owner);

    int32_t translate(Resource resource);

    virtual void onResourceReserved(ComponentType compType __unused,
                                    const sp<RMRequest> &request __unused,
                                    Resource resourceReserved __unused) {
    }


    virtual void onResourceReturned(ComponentType compType __unused,
                                    Resource resourceReturned __unused) {
    }

    // Maps to keep resource hierarchy
    KeyedVector<ComponentType, Vector<ResourceType> > mCompResourceTypeMap;
    KeyedVector<ResourceType, SortedVector<Resource> > mTypeResourceMap;
    KeyedVector<Resource, int32_t> mVendorResourceMap;

    // A map from Resource to ClientInfo that is necessary for ownership
    // transfer
    DefaultKeyedVector<Resource, sp<ClientInfo> > mResourceClientMap;

    // A map from IRMClient::asBinder() to ClientInfo which is
    // necessary for ClientInfo lookup
    DefaultKeyedVector<sp<IBinder>, sp<ClientInfo> > mClientMap;

    // A map to keep resource candidates list for each resource type.
    struct TypeCandidatesMapWithPower : public RefBase {
        ResourceType mType;
        int32_t mPower;
        Vector<std::pair<Resource, sp<ClientInfo> > > mResourceClientList;
    };

    typedef Vector<sp<TypeCandidatesMapWithPower> > TCMVector;

    bool generateResourceMapWithPower(const sp<RMRequest> &request,
                                     Vector<TCMVector> *resourceMap);


    virtual bool decideClientsToPreemptFrom(const sp<IRMClient> &client,
                                           const sp<RMRequest> &request,
                                           const Vector<TCMVector> &resourceMap,
                                           Vector< sp<ClientInfo> > *clientsMap);
    virtual bool decideClientsToPreemptFromByComponent(const sp<IRMClient> &client,
                                                      const sp<TypeCandidatesMapWithPower> &tcm,
                                                      Vector< sp<ClientInfo> > *clientsMap);


    virtual sp<RMResponse> allocatePowerResources(const sp<IRMClient> &client,
                                                 const sp<RMRequest> &request,
                                                  const Vector<TCMVector> &resourceMap);

    virtual bool allocateOnePowerResource(const sp<IRMClient> &client,
                                         const sp<TypeCandidatesMapWithPower> &tcm,
                                         const ComponentType &comp,
                                         Vector<Resource> &resources);


    virtual bool getCandidatesWithPower(ResourceType type,
                                       const sp<RMRequest> &request,
                                       Vector<Resource> *resourceCandidates,
                                       int32_t *power) = 0;

private:
    void addClientIfNecessary(Vector<sp<ClientInfo> > *clientsMap,
                             const sp<ClientInfo> &client);
};

}  // namespace android

#endif  // ANDROID_RM_ALLOCATOR_H_
