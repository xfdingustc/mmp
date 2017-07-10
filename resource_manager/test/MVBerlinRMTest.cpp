/*
 **                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
 ** THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.
 ** NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT
 ** OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE
 ** DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.
 ** THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,
 ** IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
 **
 ** MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES,
 ** MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL
 ** SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.
 ** (MJKK), MARVELL ISRAEL LTD. (MSIL).
 */

#define LOG_NDEBUG 0
#define LOG_TAG "MVBerlinRMTest"
#include <utils/Log.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

#include <utils/ValueUpdateListener_inl.h>

#include <RMClient.h>
#include <RMClientListener.h>
#include <RMRequest.h>
#include <RMResponse.h>
#include <RMTypes.h>
#include <MVBerlinRMTypes.h>

#include <amp_client.h>

#include <gtest/gtest.h>

using namespace berlin;
using namespace android;

static const uint64_t kWaitIntervalUs = 5000000L;

class MVBerlinRMClientListener : public RMClientListener {
public:
    MVBerlinRMClientListener()
        : mRequestDone(new ValueUpdateListener<bool>(false)),
          mRequestDone1(new ValueUpdateListener<bool>(false)),
          mResourcePreempted(new ValueUpdateListener<bool>(false)),
          mResourceReturned(new ValueUpdateListener<bool>(false)),
          mAVINId(-1),
          mVPP(-1),
          mOnce(false) {
    }

    virtual void handleRequestDone(const sp<RMResponse> &response) {
        if (response != NULL) {
            response->getFirstResourceFor(kComponentAVINMaster, &mAVINId);
            response->getFirstResourceFor(kComponentVideoRenderer, &mVPP);
            if (!mOnce) {
                mRequestDone->set(true);
                mOnce = true;
            } else {
                mRequestDone1->set(true);
            }
        } else {
            mRequestDone->set(false);
        }
    }

    virtual bool handleResourcePreempted() {
        mResourcePreempted->set(true);
        // In this example, we are requesting RMClient to return all the
        // resources right away since we have no actual H/W resources that need
        // to be released or destroyed.
        sp<RMClient> client = mClient.promote();
        if (client != NULL) {
            client->returnAllResourcesAsync();
        } else {
            ALOGE("Could not return resources since mClient is NULL.");
        }
        return true;
    }

    virtual void handleAllResourcesReturned() {
        mResourceReturned->set(true);
    }

    sp<ValueUpdateListener<bool> > mRequestDone;
    sp<ValueUpdateListener<bool> > mRequestDone1;
    sp<ValueUpdateListener<bool> > mResourcePreempted;
    sp<ValueUpdateListener<bool> > mResourceReturned;
    Resource mAVINId;
    Resource mVPP;
    bool mOnce;
};

class MVResTest : public testing::Test {
protected:
    virtual void SetUp() {
    }
};

TEST(MVResTest, AVIN) {
    ProcessState::self()->startThreadPool();
    sp<MVBerlinRMClientListener> listener = new MVBerlinRMClientListener();
    CHECK(listener != NULL);
    sp<RMClient> rmClient = new RMClient(listener, kRMClientPriorityMedium);
    CHECK(rmClient != NULL);

    sp<RMRequest> req = new RMRequest();
    req->addComponent(kComponentAVINMaster);

    rmClient->requestResourcesAsync(req);

    // Waits for REQUEST_DONE message
    EXPECT_EQ(true, listener->mRequestDone->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(kBerlinResourceAVInputMaster),
             static_cast<int>(listener->mAVINId));
    rmClient->returnAllResources();

    //system("dumpsys media.hw_resourcemanager");
}

TEST(MVResTest, VPP_DEINTERLACE) {
    ProcessState::self()->startThreadPool();
    sp<MVBerlinRMClientListener> listener = new MVBerlinRMClientListener();
    CHECK(listener != NULL);
    sp<RMClient> rmClient = new RMClient(listener, kRMClientPriorityMedium);
    CHECK(rmClient != NULL);

    sp<RMRequest> req = new RMRequest();
    req->addComponent(kComponentVideoRenderer);
    req->addCapability(kRMCapabilityVideoDeInterlace, 1);

    rmClient->requestResourcesAsync(req);

    // Waits for REQUEST_DONE message
    EXPECT_EQ(true, listener->mRequestDone->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(AMP_DISP_PLANE_MAIN), static_cast<int>(listener->mVPP));

    sp<MVBerlinRMClientListener> listener1 = new MVBerlinRMClientListener();
    CHECK(listener1 != NULL);
    sp<RMClient> rmClient1 = new RMClient(listener1, kRMClientPriorityMedium);
    CHECK(rmClient1 != NULL);

    sp<RMRequest> req1 = new RMRequest();
    req1->addComponent(kComponentVideoRenderer);
    req1->addCapability(kRMCapabilityVideoDeInterlace, 1);

    rmClient1->requestResourcesAsync(req1);

    EXPECT_EQ(true, listener->mResourcePreempted->waitForValue(true, kWaitIntervalUs));
    // Waits for REQUEST_DONE message
    EXPECT_EQ(true, listener1->mRequestDone->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(AMP_DISP_PLANE_MAIN), static_cast<int>(listener1->mVPP));

    rmClient1->returnAllResources();

    //system("dumpsys media.hw_resourcemanager");
}

TEST(MVResTest, VPP_PIP) {
    ProcessState::self()->startThreadPool();
    sp<MVBerlinRMClientListener> listener = new MVBerlinRMClientListener();
    CHECK(listener != NULL);
    sp<RMClient> rmClient = new RMClient(listener, kRMClientPriorityMedium);
    CHECK(rmClient != NULL);

    sp<RMRequest> req = new RMRequest();
    req->addComponent(kComponentVideoRenderer);

    rmClient->requestResourcesAsync(req);

    // Waits for REQUEST_DONE message
    EXPECT_EQ(true, listener->mRequestDone->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(AMP_DISP_PLANE_MAIN), static_cast<int>(listener->mVPP));

    sp<MVBerlinRMClientListener> listener1 = new MVBerlinRMClientListener();
    CHECK(listener1 != NULL);
    sp<RMClient> rmClient1 = new RMClient(listener1, kRMClientPriorityMedium);
    CHECK(rmClient1 != NULL);

    sp<RMRequest> req1 = new RMRequest();
    req1->addComponent(kComponentVideoRenderer);

    rmClient1->requestResourcesAsync(req1);

    EXPECT_EQ(true, listener1->mRequestDone->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(AMP_DISP_PLANE_PIP), static_cast<int>(listener1->mVPP));

    rmClient->returnAllResources();
    rmClient1->returnAllResources();

    //system("dumpsys media.hw_resourcemanager");
}

TEST(MVResTest, TWICE) {
    ProcessState::self()->startThreadPool();
    sp<MVBerlinRMClientListener> listener = new MVBerlinRMClientListener();
    CHECK(listener != NULL);
    sp<RMClient> rmClient = new RMClient(listener, kRMClientPriorityMedium);
    CHECK(rmClient != NULL);

    sp<RMRequest> req = new RMRequest();
    req->addComponent(kComponentVideoRenderer);
    req->addCapability(static_cast<RMCapabilityType>(kRMCapabilityVideoDeInterlace), 1);

    rmClient->requestResourcesAsync(req);

    // Waits for REQUEST_DONE message
    EXPECT_EQ(true, listener->mRequestDone->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(AMP_DISP_PLANE_MAIN), static_cast<int>(listener->mVPP));

    sp<RMRequest> req1 = new RMRequest();
    req1->addComponent(kComponentAVINMaster);

    rmClient->requestResourcesAsync(req1);

    EXPECT_EQ(true, listener->mRequestDone1->waitForValue(true, 5000000L));
    EXPECT_EQ(static_cast<int>(AMP_DISP_PLANE_MAIN), static_cast<int>(listener->mVPP));
    EXPECT_EQ(static_cast<int>(kBerlinResourceAVInputMaster),
             static_cast<int>(listener->mAVINId));

    rmClient->returnAllResources();

    //system("dumpsys media.hw_resourcemanager");
}
