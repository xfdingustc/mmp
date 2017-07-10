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
#define LOG_TAG "MVBerlinRMAllocator"
#include <utils/Log.h>

#include <MVBerlinRMAllocator.h>
#include <MVBerlinRMTypes.h>

#include <RMRequest.h>

#include <amp_client.h>

using namespace berlin;

namespace android {

MVBerlinRMAllocator::MVBerlinRMAllocator() {
    ALOGV("MVBerlinRMAllocator constructor");

    addResourceType(kComponentVideoRenderer, kBerlinVideoPlane);

    addResourceType(kComponentAVINMaster, kBerlinAVInputMaster);

    addResource(kBerlinVideoPlane, kBerlinResourceVideoPlane_1, AMP_DISP_PLANE_MAIN);
    addResource(kBerlinVideoPlane, kBerlinResourceVideoPlane_2, AMP_DISP_PLANE_PIP);

    addResource(kBerlinAVInputMaster, kBerlinResourceAVInputMaster);
}

bool MVBerlinRMAllocator::getCandidatesWithPower(ResourceType type,
                                                const sp<RMRequest> &request,
                                                Vector<Resource> *resourceCandidates,
                                                int32_t *power) {
    ALOGV("getCandidatesWithPower");
    int32_t de_interlace = request->getCapability(kRMCapabilityVideoDeInterlace, 0);
    int32_t is_pip = request->getCapability(kRMCapabilityVideoPIP, 0);

    // Default power is 1
    *power = 1;
    if (type == kBerlinVideoPlane) {
        if (de_interlace == 1) {
            resourceCandidates->push(kBerlinResourceVideoPlane_1);
        } else if (is_pip == 1) {
#ifdef DISABLE_PIP
            resourceCandidates->push(kBerlinResourceVideoPlane_1);
#else
            resourceCandidates->push(kBerlinResourceVideoPlane_2);
#endif
        } else {
            // Primary video has high priority.
            resourceCandidates->push(kBerlinResourceVideoPlane_1);
#ifndef DISABLE_PIP
            resourceCandidates->push(kBerlinResourceVideoPlane_2);
#endif
        }
    } else if (type == kBerlinAVInputMaster) {
        resourceCandidates->push(kBerlinResourceAVInputMaster);
    } else {
        ALOGW("We have undefined resource type: %d", type);
        return false;
    }
    return true;
}

}  // namespace android
