/*
**                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.
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

#define LOG_TAG "MVBerlinOMXPlugin"
#include <utils/Log.h>

#include <dlfcn.h>  // for dlopen, dlsym, ...

#include <media/hardware/HardwareAPI.h>

#include <media/stagefright/foundation/ADebug.h>

#include "MVBerlinOMXPlugin.h"

namespace android {

OMXPluginBase *createOMXPlugin() {
    return new MVBerlinOMXPlugin;  // deleted in OMXMaster::clearPlugins()
}

////////////////////////////////////////////////////////////////

MVBerlinOMXPlugin::MVBerlinOMXPlugin()
    : mLibHandle(NULL),
      mInitFunc(NULL),
      mDeinitFunc(NULL),
      mComponentNameEnumFunc(NULL),
      mGetHandleFunc(NULL),
      mFreeHandleFunc(NULL),
      mGetRolesOfComponentHandleFunc(NULL) {
    ALOGV("%s %d", __FUNCTION__, __LINE__);

    init();
}

MVBerlinOMXPlugin::~MVBerlinOMXPlugin() {
    ALOGV("%s %d", __FUNCTION__, __LINE__);

    deinit();
}

void MVBerlinOMXPlugin::init() {
    ALOGV("Loading vendor omx library.");
    if (mLibHandle == NULL) {
        mLibHandle = dlopen("libBerlinCore.so", RTLD_NOW);
    }

    if (mLibHandle == NULL) {
        ALOGE("Error loading libBerlinCore.so");
        return;
    }
    mInitFunc = (InitFunc)dlsym(mLibHandle, "OMX_Init");
    mDeinitFunc = (DeinitFunc)dlsym(mLibHandle, "OMX_Deinit");

    mComponentNameEnumFunc =
            (ComponentNameEnumFunc)dlsym(mLibHandle, "OMX_ComponentNameEnum");

    mGetHandleFunc = (GetHandleFunc)dlsym(mLibHandle, "OMX_GetHandle");
    mFreeHandleFunc = (FreeHandleFunc)dlsym(mLibHandle, "OMX_FreeHandle");

    mGetRolesOfComponentHandleFunc =
            (GetRolesOfComponentFunc)dlsym(mLibHandle,
                                           "OMX_GetRolesOfComponent");

    mSetupTunnelFunc =
            (SetupTunnelFunc)dlsym(mLibHandle,
                                   "OMX_SetupTunnel");

    if (mInitFunc != NULL) {
        (*mInitFunc)();
    } else {
        ALOGE("OMX_Init not implemented");
    }
}

void MVBerlinOMXPlugin::deinit() {
    ALOGV("Unloading vendor omx library.");
    if (mLibHandle != NULL) {
        (*mDeinitFunc)();

        dlclose(mLibHandle);

        mLibHandle = NULL;
    }
}

OMX_ERRORTYPE MVBerlinOMXPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component) {
    ALOGV("%s %d", __FUNCTION__, __LINE__);

    if (mLibHandle == NULL) return OMX_ErrorUndefined;

    return (*mGetHandleFunc)(
            reinterpret_cast<OMX_HANDLETYPE *>(component),
            const_cast<char *>(name),
            appData, const_cast<OMX_CALLBACKTYPE *>(callbacks));
}

OMX_ERRORTYPE MVBerlinOMXPlugin::destroyComponentInstance(
        OMX_COMPONENTTYPE *component) {
    ALOGV("%s %d", __FUNCTION__, __LINE__);
    if (mLibHandle == NULL) return OMX_ErrorUndefined;

    return (*mFreeHandleFunc)(reinterpret_cast<OMX_HANDLETYPE *>(component));
}

OMX_ERRORTYPE MVBerlinOMXPlugin::enumerateComponents(
        OMX_STRING name,
        size_t size,
        OMX_U32 index) {
    ALOGV("%s %d", __FUNCTION__, __LINE__);

    if (mLibHandle == NULL) return OMX_ErrorUndefined;

    return (*mComponentNameEnumFunc)(name, size, index);
}

OMX_ERRORTYPE MVBerlinOMXPlugin::getRolesOfComponent(
        const char *name,
        Vector<String8> *roles) {
    ALOGV("%s %d", __FUNCTION__, __LINE__);
    roles->clear();

    if (mLibHandle == NULL) return OMX_ErrorUndefined;

    // Retrieves numRoles only.
    OMX_U32 numRoles;
    OMX_ERRORTYPE err = (*mGetRolesOfComponentHandleFunc)(
            const_cast<OMX_STRING>(name), &numRoles, NULL);

    if (err != OMX_ErrorNone) return err;

    if (numRoles > 0) {
        OMX_U8 **array = new OMX_U8 *[numRoles];
        for (OMX_U32 i = 0; i < numRoles; ++i) {
            array[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
        }

        // Puts roles in the array.
        OMX_U32 numRoles2 = numRoles;
        err = (*mGetRolesOfComponentHandleFunc)(
                const_cast<OMX_STRING>(name), &numRoles2, array);

        CHECK_EQ(err, OMX_ErrorNone);
        CHECK_EQ(numRoles, numRoles2);

        for (OMX_U32 i = 0; i < numRoles; ++i) {
            String8 s(reinterpret_cast<const char *>(array[i]));
            roles->push(s);

            delete [] array[i];
            array[i] = NULL;
        }

        delete [] array;
        array = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MVBerlinOMXPlugin::setupTunnel(
        OMX_COMPONENTTYPE *outputComponent, OMX_U32 outputPortIndex,
        OMX_COMPONENTTYPE *inputComponent, OMX_U32 inputPortIndex) {
    return (*mSetupTunnelFunc)(
            outputComponent, outputPortIndex, inputComponent, inputPortIndex);
}

}  // namespace android
