/*
**                Copyright 2014, MARVELL SEMICONDUCTOR, LTD.
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

#define LOG_TAG "OmxCore"

#include <dlfcn.h>  // for dlopen, dlsym, ...
#include <string.h>

#include <vector>
#include <string>

#include <OmxCore.h>
#include <OmxComponent.h>
#include <OmxComponentFactory.h>

namespace mmp {

OmxCore* OmxCore::sOmxCore_ = NULL;

typedef struct DynamicComponentEntry_ {
  const char * compName;
}DynamicComponentEntry;

static DynamicComponentEntry gDymamicComponents[] = {
  // Audio
  "comp.sw.audio_decoder.mp3",
  "comp.sw.audio_decoder.aac",
  "comp.sw.audio_decoder.vorbis",
  // Video
  "comp.sw.video_decoder.avc",
  "comp.sw.video_decoder.mpeg4",
};

OmxCore* OmxCore::getInstance() {
  if (NULL == sOmxCore_) {
    sOmxCore_ = new OmxCore();
  }
  return sOmxCore_;
}

OmxCore::OmxCore()
    : mInited(OMX_FALSE) {
}

OmxCore::~OmxCore() {
  deinit();
}

OMX_ERRORTYPE OmxCore::deinit() {
  if (mInited == OMX_TRUE) {
    while (!mHandleVector.empty()) {
      vector<OMX_HANDLETYPE>::iterator it;
      OMX_ERRORTYPE result = OMX_ErrorNone;
      it = mHandleVector.begin();
      OMX_LOGD("Free Handle %p", *it);
      result = static_cast<OMX_COMPONENTTYPE *>(*it)->ComponentDeInit(*it);
      delete static_cast<OMX_COMPONENTTYPE *>(*it);
      mHandleVector.erase(it);
    };
    while (!mComponentVector.empty()) {
      vector<ComponentEntry>::iterator it;
      it = mComponentVector.begin();
      mComponentVector.erase(it);
    };
    mInited = OMX_FALSE;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCore::init() {
  if (mInited == OMX_FALSE) {
    for (OMX_U32 i = 0; i< NELM(gDymamicComponents); i++) {
      ComponentEntry comp_entry;
      strncpy(comp_entry.name, gDymamicComponents[i].compName,
          OMX_MAX_STRINGNAME_SIZE);
      mComponentVector.push_back(comp_entry);
    }
    mInited = OMX_TRUE;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCore::getHandle(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  if (strncmp(componentName, "comp.sw", 7)) {
    OMX_LOGE("Can't create component %s in this OMX Core.", componentName);
    return OMX_ErrorInvalidComponentName;
  }

  vector<ComponentEntry>::iterator it;
  for (it = mComponentVector.begin(); it < mComponentVector.end(); it++) {
    if (!strcmp(it->name, componentName)) {
      result = OmxComponentFactory::CreateComponentEntry(
          handle, componentName, appData, callBacks);
      mHandleVector.push_back(*handle);
      break;
    }
  }
  if (it == mComponentVector.end()) {
    OMX_LOGE("Can't create component %s in this OMX Core.", componentName);
    return OMX_ErrorComponentNotFound;
  }

  return result;
}

OMX_ERRORTYPE OmxCore::freeHandle(OMX_HANDLETYPE hComponent) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  vector<OMX_HANDLETYPE>::iterator it;
  for (it = mHandleVector.begin(); it < mHandleVector.end(); it++) {
    if (*it == hComponent) {
      result = static_cast<OMX_COMPONENTTYPE*>(hComponent)->ComponentDeInit(hComponent);
      delete reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent);
      mHandleVector.erase(it);
      break;
    }
  }
  return result;
}

OMX_ERRORTYPE OmxCore::setupTunnel(
    OMX_HANDLETYPE hOutput,
    OMX_U32 nPortOutput,
    OMX_HANDLETYPE hInput,
    OMX_U32 nPortInput) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_COMPONENTTYPE *pOutputComp = static_cast<OMX_COMPONENTTYPE *>(hOutput);
  OMX_COMPONENTTYPE *pInputComp = static_cast<OMX_COMPONENTTYPE *>(hInput);
  OMX_TUNNELSETUPTYPE tunnelSetup;
  tunnelSetup.nTunnelFlags = 0;
  tunnelSetup.eSupplier = OMX_BufferSupplyUnspecified;

  result = pOutputComp->ComponentTunnelRequest(hOutput, nPortOutput, hInput,
      nPortInput, &tunnelSetup);

  if (OMX_ErrorNone != result) {
    return result;
  }

  return pInputComp->ComponentTunnelRequest(hInput, nPortInput, hOutput,
      nPortOutput, &tunnelSetup);
}

OMX_ERRORTYPE OmxCore::componentNameEnum(
    OMX_STRING componentName,
    OMX_U32 nameLength,
    OMX_U32 index) {
  if (index >= mComponentVector.size()) {
    return OMX_ErrorNoMore;
  }
  OMX_STRING name = mComponentVector.at(index).name;
  if (nameLength <= strlen(name)) {
    return OMX_ErrorBadParameter;
  } else {
    strncpy(componentName, name, nameLength);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCore::getComponentsOfRole(
    OMX_STRING role,
    OMX_U32 *pNumComps,
    OMX_U8 **compNames) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_U32 nNumComps = 0;

  vector<ComponentEntry>::iterator it;
  for (it = mComponentVector.begin(); it < mComponentVector.end(); it++) {
    OMX_HANDLETYPE handle;
    result = OmxComponentFactory::CreateComponentEntry(&handle, it->name, NULL, NULL);
    if (OMX_ErrorNone != result) {
      return result;
    }
    OMX_U32 i;
    OMX_COMPONENTTYPE *comp = reinterpret_cast<OMX_COMPONENTTYPE *>(handle);
    if (compNames == NULL) {
      OMX_ERRORTYPE eError = OMX_ErrorNone;
      OMX_U8 role_ret[OMX_MAX_STRINGNAME_SIZE];
      for (i = 0; OMX_ErrorNoMore != eError; i++) {
        eError = comp->ComponentRoleEnum(handle, role_ret, i);
        if (!strncmp(role, (OMX_STRING)role_ret, OMX_MAX_STRINGNAME_SIZE)) {
          nNumComps++;
          break;
        }
      }
    } else {
      OMX_ERRORTYPE eError = OMX_ErrorNone;
      for (i = 0; OMX_ErrorNoMore != eError ; i++) {
        OMX_U8 role_ret[OMX_MAX_STRINGNAME_SIZE];
        eError = comp->ComponentRoleEnum(handle, role_ret, i);
        if (!strncmp(role, (OMX_STRING)role_ret, OMX_MAX_STRINGNAME_SIZE)) {
          strncpy((OMX_STRING)compNames[nNumComps], it->name,
              OMX_MAX_STRINGNAME_SIZE);
          nNumComps++;
          break;
        }
      }
    }
    result = freeHandle(handle);
    if (compNames != NULL && nNumComps >= *pNumComps) {
      return OMX_ErrorNone;
    }
  }
  *pNumComps = nNumComps;

  return result;
}

OMX_ERRORTYPE OmxCore::getRolesOfComponent(
    OMX_STRING compName,
    OMX_U32 *pNumRoles,
    OMX_U8 **roles) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  vector<ComponentEntry>::iterator it;
  for (it = mComponentVector.begin(); it != mComponentVector.end(); it++) {
    if (!strcmp(it->name, compName)) {
      OMX_HANDLETYPE handle;
      result = OmxComponentFactory::CreateComponentEntry(&handle, compName, NULL, NULL);
      if (OMX_ErrorNone != result) {
        return result;
      }

      OMX_U32 i;
      OMX_ERRORTYPE eError = OMX_ErrorNone;
      OMX_COMPONENTTYPE *comp = reinterpret_cast<OMX_COMPONENTTYPE *>(handle);
      if (roles == NULL) {
        OMX_U8 role[OMX_MAX_STRINGNAME_SIZE];
        for (i = 0; OMX_ErrorNoMore != eError; i++) {
          eError = comp->ComponentRoleEnum(handle, role, i);
          OMX_LOGV("Role %d, %s, err %x", i, role, eError);
        }
        *pNumRoles = i - 1;
      } else {
        for (i = 0; i < *pNumRoles ; i++) {
          eError = comp->ComponentRoleEnum(handle, roles[i], i);
          OMX_LOGV("Copy Role %d, %s", i, roles[i], eError);
          if (eError == OMX_ErrorNoMore) {
            break;
          }
        }
        *pNumRoles = i;
      }
      OMX_LOGV("Comp %s, has %d roles", compName, *pNumRoles);
      result = static_cast<OMX_COMPONENTTYPE*>(handle)->ComponentDeInit(handle);
      delete reinterpret_cast<OMX_COMPONENTTYPE *>(handle);
      break;
    }
  }
  return result;
}

OMX_ERRORTYPE OmxCore::getContentPipe(
    OMX_HANDLETYPE *hPipe,
    OMX_STRING szURI) {
  return OMX_ErrorNotImplemented;
}

}

extern "C" {

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void) {
  return mmp::OmxCore::getInstance()->init();
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void) {
  return mmp::OmxCore::getInstance()->deinit();
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN OMX_U32 nNameLength,
    OMX_IN OMX_U32 nIndex) {
  return mmp::OmxCore::getInstance()->componentNameEnum(cComponentName,
                                                    nNameLength, nIndex);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN OMX_STRING cComponentName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE* pCallBacks) {
  return mmp::OmxCore::getInstance()->getHandle(
      pHandle, cComponentName, pAppData, pCallBacks);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN OMX_HANDLETYPE hComponent) {
  return mmp::OmxCore::getInstance()->freeHandle(hComponent);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput) {
  return mmp::OmxCore::getInstance()->setupTunnel(
      hOutput, nPortOutput, hInput, nPortInput);
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole(
    OMX_IN      OMX_STRING role,
    OMX_INOUT   OMX_U32 *pNumComps,
    OMX_INOUT   OMX_U8  **compNames) {
  return mmp::OmxCore::getInstance()->getComponentsOfRole(
      role, pNumComps, compNames);
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent(
    OMX_IN      OMX_STRING compName,
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles) {
  return mmp::OmxCore::getInstance()->getRolesOfComponent(
      compName, pNumRoles, roles);
}

}
