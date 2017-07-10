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
#define LOG_TAG "DynamicOmxCore"
#include <DynamicOmxCore.h>
#include <dlfcn.h>  // for dlopen, dlsym, ...

#include <stdio.h>
#include <cstdlib>
namespace berlin {

  DynamicOmxCore::DynamicOmxCore() :
    mCoreHandle(NULL),
    mInited(OMX_FALSE) {
}

DynamicOmxCore::DynamicOmxCore(OMX_STRING coreName) :
    mInited(OMX_FALSE) {
  mCoreHandle = dlopen(coreName, RTLD_NOW);
}

DynamicOmxCore::~DynamicOmxCore() {
  if (mCoreHandle != NULL) {
    if (mInited) {
      (*mDeinitFunc)();
      mInited = OMX_FALSE;
    }
    dlclose(mCoreHandle);
    mCoreHandle = NULL;
  }
}

OMX_ERRORTYPE DynamicOmxCore::init() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (NULL == mCoreHandle) {
    return OMX_ErrorUndefined;
  }
  if (OMX_FALSE == mInited) {
    mInitFunc = (InitFunc)dlsym(mCoreHandle, "OMX_Init");
    mDeinitFunc = (DeinitFunc)dlsym(mCoreHandle, "OMX_Deinit");
    mComponentNameEnumFunc =
        (ComponentNameEnumFunc)dlsym(mCoreHandle, "OMX_ComponentNameEnum");
    mGetHandleFunc = (GetHandleFunc)dlsym(mCoreHandle, "OMX_GetHandle");
    mFreeHandleFunc = (FreeHandleFunc)dlsym(mCoreHandle, "OMX_FreeHandle");
    mGetRolesOfComponentFunc =
        (GetRolesOfComponentFunc)dlsym(mCoreHandle, "OMX_GetRolesOfComponent");
    mGetComponentsOfRoleFunc =
        (GetComponentsOfRoleFunc)dlsym(mCoreHandle, "OMX_GetComponentsOfRole");
    mSetupTunnelFunc = (SetupTunnelFunc)dlsym(mCoreHandle, "OMX_SetupTunnel");
    if (mInitFunc != NULL) {
      err = (*mInitFunc)();
      if (err == OMX_ErrorNone)  {
        mInited = OMX_TRUE;
      }
    } else {
      return OMX_ErrorUndefined;
    }
  }
  return err;
}

OMX_ERRORTYPE DynamicOmxCore::deinit() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mCoreHandle != NULL) {
    if (mInited) {
      err = (*mDeinitFunc)();
      mInited = OMX_FALSE;
    }
  }
  return err;
}

OMX_ERRORTYPE DynamicOmxCore::getHandle(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  return (*mGetHandleFunc)(handle, componentName, appData, callBacks);
}

OMX_ERRORTYPE DynamicOmxCore::freeHandle(OMX_HANDLETYPE hComponent) {
  return (*mFreeHandleFunc)(hComponent);
}

OMX_ERRORTYPE DynamicOmxCore::setupTunnel(
    OMX_HANDLETYPE hOutput,
    OMX_U32 nPortOutput,
    OMX_HANDLETYPE hInput,
    OMX_U32 nPortInput) {
  return (*mSetupTunnelFunc)(hOutput, nPortOutput, hInput, nPortInput);
}

OMX_ERRORTYPE DynamicOmxCore::componentNameEnum(
    OMX_STRING componentName,
    OMX_U32 nameLength,
    OMX_U32 index) {
  return (*mComponentNameEnumFunc)(componentName, nameLength, index);
}

OMX_ERRORTYPE DynamicOmxCore::getComponentsOfRole(
    OMX_STRING role,
    OMX_U32 *pNumComps,
    OMX_U8 **compNames) {
  return (*mGetComponentsOfRoleFunc)(role, pNumComps, compNames);
}

OMX_ERRORTYPE DynamicOmxCore::getRolesOfComponent(
    OMX_STRING compName,
    OMX_U32 *pNumRoles,
    OMX_U8 **roles) {
  return (*mGetRolesOfComponentFunc)(compName, pNumRoles, roles);
}

OMX_ERRORTYPE DynamicOmxCore::getContentPipe(
    OMX_HANDLETYPE *hPipe,
    OMX_STRING szURI) {
  return OMX_ErrorNone;
}

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
OMX_ERRORTYPE DynamicOmxCore::teardownTunnel(
    OMX_HANDLETYPE hOutput,
    OMX_U32 nPortOutput,
    OMX_HANDLETYPE hInput,
    OMX_U32 nPortInput) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE DynamicOmxCore::enumComponentOfRole(
    OMX_STRING compName,
    OMX_STRING role,
    OMX_U32 nIndex) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE DynamicOmxCore::enumRoleOfComponent(
    OMX_STRING role,
    OMX_STRING compName,
    OMX_U32 nIndex) {
  return OMX_ErrorNone;
}
#endif // OMX_SPEC_1_2_0_0_SUPPORT
}

