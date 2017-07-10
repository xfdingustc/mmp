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
#define LOG_TAG "BerlinOmxCore"

extern "C" {
#include <amp_client_support.h>
#include <OSAL_api.h>
}

#include <dlfcn.h>  // for dlopen, dlsym, ...
#include <string.h>

#include <BerlinOmxCore.h>
#include <BerlinOmxComponent.h>
#include <DynamicOmxCore.h>
#include <vector>
#include <string>


#define EXPORT_OMX_CORE_MASTER

namespace berlin {

static OmxCoreMaster *gOmxCoreMaster = NULL;
static BerlinCore *gBerlinCore = NULL;
// Mutex gOmxCoreMasterLock;
static OMX_U32 gOmxCoreMasterUseCount = 0;
static const char *kBellagioCoreName = "libbellagiocore.so";
static char *client_argv[] = {"client", "iiop:1.0//127.0.0.1:999/AMP::FACTORY/factory"};

typedef struct DynamicComponentEntry_ {
  const char * compName;
  const char * libName;
}DynamicComponentEntry;

#ifdef _WIN32
static DynamicComponentEntry gDymamicComponents[] = {
  {"OMX.Marvell.video_decoder.avc", "BerlinVideo.dll"},
};
#else
static DynamicComponentEntry gDymamicComponents[] = {
  {"OMX.Marvell.video_decoder.avc", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.avc.secure", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.h263", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.mpeg2", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.mpeg4", "libBerlinVideo.so"},
#ifdef _ANDROID_
  {"OMX.Marvell.video_decoder.vpx", "libBerlinVideo.so"},
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  {"OMX.Marvell.video_decoder.vp8", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.vc1", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.vc1.secure", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.msmpeg4", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.hevc", "libBerlinVideo.so"},
#endif
  {"OMX.Marvell.video_decoder.mjpeg", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.wmv", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.wmv.secure", "libBerlinVideo.so"},
  {"OMX.Marvell.video_decoder.rv", "libBerlinVideo.so"},
  {"OMX.Marvell.video_encoder.avc", "libBerlinVideo.so"},
  {"OMX.Marvell.video_encoder.mpeg4", "libBerlinVideo.so"},
  {"OMX.Marvell.video_encoder.vp8", "libBerlinVideo.so"},
  {"OMX.Marvell.iv_processor.yuv", "libBerlinVideo.so"},
  {"OMX.Marvell.iv_renderer.yuv.overlay", "libBerlinVideo.so"},
  {"OMX.Marvell.video_scheduler.binary", "libBerlinVideo.so"},
  {"OMX.Marvell.audio_decoder.aac", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.aac.secure", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.vorbis", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.g711", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.ac3", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.ac3.secure", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.amrnb", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.amrwb", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.dts", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.wma", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.wma.secure", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.mp3", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.mp2", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.mp1", "libBerlinAudio.so"},
  {"OMX.Marvell.audio_decoder.raw", "libBerlinAudio.so"},
#ifdef ENABLE_EXT_RA
  {"OMX.Marvell.audio_decoder.ra", "libBerlinAudio.so"},
#endif
  {"OMX.Marvell.audio_renderer.pcm", "libBerlinAudio.so"},
  {"OMX.Marvell.clock.binary", "libBerlinClock.so"},
};
#endif

BerlinCore::BerlinCore()
    : mInited(OMX_FALSE) {
  AMP_FACTORY hFactory = NULL;
  AMP_GetFactory(&hFactory);
  if (hFactory == NULL) {
    OMX_LOGD("Initialize amp client");
    MV_OSAL_Init();
    AMP_Initialize(2, client_argv, NULL);
  }
}

BerlinCore::~BerlinCore() {
  deinit();
}

OMX_ERRORTYPE BerlinCore::deinit() {
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
      if (it->libHandle != NULL) {
        dlclose(it->libHandle);
      }
      mComponentVector.erase(it);
    };
    mInited = OMX_FALSE;
  }
  // MV_OSAL_Exit();
  return OMX_ErrorNone;
}

OMX_ERRORTYPE BerlinCore::init() {
  if (mInited == OMX_FALSE) {
    size_t i;
    for (i = 0; i< NELM(gDymamicComponents); i++) {
      ComponentEntry comp_entry;
      comp_entry.libHandle = dlopen(gDymamicComponents[i].libName, RTLD_NOW);
      if (comp_entry.libHandle == NULL) {
        OMX_LOGE("%s", dlerror());
        continue;
      }
      comp_entry.createFunc = (createComponentFunc)dlsym(
          comp_entry.libHandle, "CreateComponentEntry");

      strncpy(comp_entry.name, gDymamicComponents[i].compName,
          OMX_MAX_STRINGNAME_SIZE);
      mComponentVector.push_back(comp_entry);
    }
    mInited = OMX_TRUE;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE BerlinCore::getHandle(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  if (strncmp(componentName, "OMX", 3)) {
    return OMX_ErrorInvalidComponentName;
  }
  vector<ComponentEntry>::iterator it;

  for (it = mComponentVector.begin(); it < mComponentVector.end(); it++) {
    if (!strcmp(it->name, componentName)) {
      result = it->createFunc(handle, componentName, appData, callBacks);
      mHandleVector.push_back(*handle);
      break;
    }
  }
  if (it == mComponentVector.end()) {
    return OMX_ErrorComponentNotFound;
  }
  return result;
}

OMX_ERRORTYPE BerlinCore::freeHandle(OMX_HANDLETYPE hComponent) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  vector<OMX_HANDLETYPE>::iterator it;
  for (it = mHandleVector.begin(); it < mHandleVector.end(); it++) {
    if (*it == hComponent) {
      result = static_cast<OMX_COMPONENTTYPE*>(hComponent)->ComponentDeInit(
          hComponent);
      delete reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent);
      mHandleVector.erase(it);
      break;
    }
  }
  return result;
}

OMX_ERRORTYPE BerlinCore::setupTunnel(
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
  // return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE BerlinCore::componentNameEnum(
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

OMX_ERRORTYPE BerlinCore::getComponentsOfRole(
    OMX_STRING role,
    OMX_U32 *pNumComps,
    OMX_U8 **compNames) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  vector<ComponentEntry>::iterator it;
  OMX_U32 nNumComps = 0;
  for (it = mComponentVector.begin(); it < mComponentVector.end(); it++) {
    OMX_HANDLETYPE handle;
    result = it->createFunc(&handle, it->name, NULL, NULL);
    if (OMX_ErrorNone != result)
      return result;
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
OMX_ERRORTYPE BerlinCore::getRolesOfComponent(
    OMX_STRING compName,
    OMX_U32 *pNumRoles,
    OMX_U8 **roles) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  vector<ComponentEntry>::iterator it;
  for (it = mComponentVector.begin(); it != mComponentVector.end(); it++) {
    if (!strcmp(it->name, compName)) {
      OMX_HANDLETYPE handle;
      result = it->createFunc(&handle, compName, NULL, NULL);
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
      result = static_cast<OMX_COMPONENTTYPE*>(handle)->ComponentDeInit(
          handle);
      delete reinterpret_cast<OMX_COMPONENTTYPE *>(handle);
      break;
    }
  }
  return result;
}

OMX_ERRORTYPE BerlinCore::getContentPipe(
    OMX_HANDLETYPE *hPipe,
    OMX_STRING szURI) {
  return OMX_ErrorNotImplemented;
}

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
OMX_ERRORTYPE BerlinCore::teardownTunnel(
    OMX_HANDLETYPE hOutput,
    OMX_U32 nPortOutput,
    OMX_HANDLETYPE hInput,
    OMX_U32 nPortInput) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE BerlinCore::enumComponentOfRole(
    OMX_STRING compName,
    OMX_STRING role,
    OMX_U32 nIndex) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE BerlinCore::enumRoleOfComponent(
    OMX_STRING role,
    OMX_STRING compName,
    OMX_U32 nIndex) {
  return OMX_ErrorNone;
}
#endif  // OMX_SPEC_1_2_0_0_SUPPORT

OmxCoreMaster* getOmxCoreMaster() {
  if (NULL != gOmxCoreMaster)
    return gOmxCoreMaster;
  // AutoMutex _l(gOmxCoreMasterLock);
  gOmxCoreMaster = new OmxCoreMaster;
  return gOmxCoreMaster;
}

BerlinCore* getBerlinCore() {
  if (NULL != gBerlinCore) {
    return gBerlinCore;
  }
  gBerlinCore = new BerlinCore;
  return gBerlinCore;
}

OmxCoreMaster::OmxCoreMaster() {
  OMX_HANDLETYPE pomxcore1;
  OMX_HANDLETYPE pomxcore2;
  loadCore(&pomxcore1);
  // loadCore(const_cast<char *>(kBellagioCoreName), &pomxcore2);
}

OmxCoreMaster::~OmxCoreMaster() {
  while (!mCompHandleList.empty()) {
    vector<CompHandleInfo*>::iterator it;
    it = mCompHandleList.begin();
    OMX_ERRORTYPE err =  (*it)->mCore->freeHandle((*it)->mHandle);
    if (OMX_ErrorNone != err) {
      OMX_LOGW("%s: freeHandle %p, err %x", __func__, (*it)->mHandle, err);
    }
    delete (*it);
    mCompHandleList.erase(it);
  };
  while (!mCoreList.empty()) {
    vector<OmxCore *>::iterator it;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    it = mCoreList.begin();
    (*it)->deinit();
    mCoreList.erase(it);
  };
  while (!mRoleInfoList.empty()) {
    vector<roleInfo*>::iterator it;
    it = mRoleInfoList.begin();
    delete (*it)->name;
    while (!(*it)->componentList.empty()) {
      vector<OmxNameString*>::iterator it2;
      it2 = (*it)->componentList.begin();
      delete (*it2);
       (*it)->componentList.erase(it2);
    };
    mRoleInfoList.erase(it);
  };

  while (!mComponentInfoList.empty()) {
    vector<componentInfo*>::iterator it;
    it = mComponentInfoList.begin();
    delete (*it)->name;
    while (!(*it)->roleList.empty()) {
      vector<OmxNameString*>::iterator it2;
      it2 = (*it)->roleList.begin();
      delete (*it2);
      (*it)->roleList.erase(it2);
    };
    mComponentInfoList.erase(it);
  };
}

OMX_ERRORTYPE OmxCoreMaster::componentNameEnum(
    OMX_STRING componentName,
    OMX_U32 nameLength,
    OMX_U32 index) {
  if (NULL == componentName) {
    return OMX_ErrorBadParameter;
  }
  if (index >= mComponentInfoList.size()) {
    return OMX_ErrorNoMore;
  }
  if (nameLength <= mComponentInfoList[index]->name->getLength()) {
    return OMX_ErrorBadParameter;
  }
  strncpy(componentName, mComponentInfoList[index]->name->getString(),
      mComponentInfoList[index]->name->getLength() + 1);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCoreMaster::getComponent(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  if (!handle || !componentName) {
    return OMX_ErrorBadParameter;
  }
  *handle = NULL;
  for (vector<OmxCore*>::iterator it = mCoreList.begin(); it != mCoreList.end();
       ++it) {
    ret = (*it)->getHandle(handle, componentName, appData, callBacks);
    if (NULL != *handle) {
      mCompHandleList.push_back(new CompHandleInfo(*handle, *it));
      return OMX_ErrorNone;
    }
  }
  return OMX_ErrorComponentNotFound;
}

OMX_ERRORTYPE OmxCoreMaster::freeComponent(OMX_HANDLETYPE hComponent) {
  if (!hComponent) {
    return OMX_ErrorBadParameter;
  }
  for (vector<CompHandleInfo *>::iterator it = mCompHandleList.begin();
       it != mCompHandleList.end(); ++it) {
    if ((*it)->mHandle == hComponent) {
      OMX_ERRORTYPE err = (*it)->mCore->freeHandle(hComponent);
      if (OMX_ErrorNone == err) {
        delete (*it);
        mCompHandleList.erase(it);
        return OMX_ErrorNone;
      }
    }
  }
  return OMX_ErrorComponentNotFound;
}

OMX_ERRORTYPE OmxCoreMaster::setupTunnel(
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
  OMX_LOGV("[%s: %d], [%p: %d] -> [%p: %d]", __FUNCTION__, __LINE__,
      hOutput, nPortOutput, hInput, nPortInput);
  result = pOutputComp->ComponentTunnelRequest(hOutput, nPortOutput, hInput,
      nPortInput, &tunnelSetup);

  if (OMX_ErrorNone != result) {
    return result;
  }
  OMX_LOGV("[%s: %d]", __FUNCTION__, __LINE__);
  return pInputComp->ComponentTunnelRequest(hInput, nPortInput, hOutput,
      nPortOutput, &tunnelSetup);
}

OMX_ERRORTYPE OmxCoreMaster::getComponentsOfRole(
    OMX_STRING role,
    OMX_U32 *pNumComps,
    OMX_U8 **compNames) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  if (NULL == pNumComps) {
    return OMX_ErrorBadParameter;
  }

  OMX_U32 nNumComps = 0;
  for (vector<roleInfo*>::const_iterator it = mRoleInfoList.begin();
       it!= mRoleInfoList.end(); ++it) {
    if (!strcmp((*it)->name->getString(), role)) {
      *pNumComps = (*it)->componentList.size();
      if (compNames == NULL) {
        return OMX_ErrorNone;
      }
      OMX_U32 i;
      for (i = 0; i < *pNumComps ; i++) {
        OmxNameString *comp = (*it)->componentList[i];
        strncpy((OMX_STRING)compNames[i], comp->getString(),
            comp->getLength() + 1);
      }
      return OMX_ErrorNone;
    }
  }
  return result;
}

OMX_ERRORTYPE OmxCoreMaster::getRolesOfComponent(
    OMX_STRING compName,
    OMX_U32 *pNumRoles,
    OMX_U8 **roles) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  if (NULL == pNumRoles) {
    return OMX_ErrorBadParameter;
  }
  for (vector<componentInfo*>::const_iterator it = mComponentInfoList.begin();
       it != mComponentInfoList.end(); ++it) {
    if (!strcmp((*it)->name->getString(), compName)) {
       *pNumRoles = (*it)->roleList.size();
      if (roles == NULL) {
        return OMX_ErrorNone;
      }
      OMX_U32 i;
      for (i = 0; i < *pNumRoles ; i++) {
        OmxNameString *role = (*it)->roleList[i];
        strncpy((OMX_STRING)roles[i], role->getString(), role->getLength() + 1);
      }
      return OMX_ErrorNone;
    }
  }
  return result;
}

OMX_ERRORTYPE OmxCoreMaster::recordComponentAndRole(OmxCore *core) {
  OMX_LOGD("In %s %d", __FUNCTION__, __LINE__);
  OMX_U32 componentIndex = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  char componentName[OMX_MAX_STRINGNAME_SIZE];
  while (OMX_ErrorNoMore != core->componentNameEnum(
             componentName, OMX_MAX_STRINGNAME_SIZE, componentIndex)) {
    OMX_U32 roleNum = 0;
    ret = core->getRolesOfComponent(componentName, &roleNum, NULL);
    OMX_LOGD("Comp %s, have %lu roles", componentName, roleNum);
    if (OMX_ErrorNone == ret && roleNum > 0) {
      char** roles = new char*[roleNum];
      componentInfo* comp_info = new componentInfo;
      comp_info->name = new OmxNameString(componentName);
      for (OMX_U32 idx = 0; idx < roleNum; idx++) {
        roles[idx] = new char[OMX_MAX_STRINGNAME_SIZE];
      }
      ret = core->getRolesOfComponent(componentName, &roleNum,
          reinterpret_cast<OMX_U8 **>(roles));

      for (OMX_U32 idx = 0; idx < roleNum; idx++) {
        OMX_BOOL roleIsExisting = OMX_FALSE;
        comp_info->roleList.push_back(new OmxNameString(roles[idx]));
        for (vector<roleInfo*>::const_iterator it = mRoleInfoList.begin();
             it != mRoleInfoList.end(); ++it) {
          if (!strcmp(roles[idx], (*it)->name->getString())) {
            (*it)->componentList.push_back(new OmxNameString(componentName));
            roleIsExisting = OMX_TRUE;
            break;
          }
        }
        if (!roleIsExisting) {
          roleInfo* role_info = new roleInfo;
          role_info->name = new OmxNameString(roles[idx]);
          role_info->componentList.push_back(new OmxNameString(componentName));
          mRoleInfoList.push_back(role_info);
          mRoleNum++;
        }
      }
      for (OMX_U32 idx = 0; idx < roleNum; idx++) {
        delete[] (roles[idx]);
      }
      delete[] roles;
      mComponentInfoList.push_back(comp_info);
      mComponentNum++;
    }
    componentIndex++;
  }
  OMX_LOGD("Out %s %d", __FUNCTION__, __LINE__);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE OmxCoreMaster::dumpComponentList() {
  OMX_LOGD("In %s %d", __FUNCTION__, __LINE__);
  for (vector<componentInfo*>::const_iterator it = mComponentInfoList.begin();
       it != mComponentInfoList.end(); ++it) {
    OMX_LOGD("Component:%s", (*it)->name->getString());
    for (NameList::const_iterator role =((*it)->roleList).begin();
        role != ((*it)->roleList).end(); ++role) {
      OMX_LOGD("    %s", (*role)->getString());
    }
  }
  OMX_LOGD("Out %s %d", __FUNCTION__, __LINE__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCoreMaster::dumpRoleList() {
  for (vector<roleInfo*>::const_iterator it = mRoleInfoList.begin();
       it != mRoleInfoList.end(); ++it) {
    OMX_LOGV("Role:%s", (*it)->name->getString());
    OMX_LOGD("================================");
    for (NameList::const_iterator comp =((*it)->componentList).begin();
        comp != ((*it)->componentList).end(); ++comp) {
      OMX_LOGD("    %s", (*comp)->getString());
    }
    OMX_LOGD("================================");
  }
  OMX_LOGV("Out %s %d", __FUNCTION__, __LINE__);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE OmxCoreMaster::loadCore(OMX_STRING coreName,
    OMX_HANDLETYPE* coreHandle) {
  OMX_LOGD("In %s %d", __FUNCTION__, __LINE__);
  OmxCore *pcore = new DynamicOmxCore(coreName);
  if (NULL == pcore) {
    return OMX_ErrorComponentNotFound;
  }
  pcore->init();
  mCoreList.push_back(pcore);
  *coreHandle=(OMX_HANDLETYPE)pcore;
  recordComponentAndRole(pcore);
  // dumpComponentList();
  // dumpRoleList();
  OMX_LOGD("Out %s %d", __FUNCTION__, __LINE__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCoreMaster::loadCore(OMX_HANDLETYPE* coreHandle) {
  OMX_LOGD("In %s %d", __FUNCTION__, __LINE__);
  OmxCore *pcore= new BerlinCore;
  if (NULL == pcore) {
    return OMX_ErrorComponentNotFound;
  }
  pcore->init();
  mCoreList.push_back(pcore);
  *coreHandle=(OMX_HANDLETYPE)pcore;
  recordComponentAndRole(pcore);
  // dumpComponentList();
  // dumpRoleList();
  OMX_LOGD("Out %s %d", __FUNCTION__, __LINE__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCoreMaster::unloadCore(OMX_HANDLETYPE coreHandle) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OmxCore *core = static_cast<OmxCore *>(coreHandle);
  ret = core->deinit();
  vector<OmxCore *>::iterator it = mCoreList.begin();
  for (; it < mCoreList.end(); it++) {
    if (core == *it) {
      mCoreList.erase(it);
      break;
    }
  }
  return OMX_ErrorNone;
}

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
OMX_ERRORTYPE OmxCoreMaster::teardownTunnel(
    OMX_HANDLETYPE hOutput,
    OMX_U32 nPortOutput,
    OMX_HANDLETYPE hInput,
    OMX_U32 nPortInput) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxCoreMaster::enumComponentsOfRole(
    OMX_STRING compName,
    OMX_STRING role,
    OMX_U32 nIndex) {
  if (NULL == compName) {
    return OMX_ErrorBadParameter;
  }
  for (vector<roleInfo*>::const_iterator it = mRoleInfoList.begin();
       it!= mRoleInfoList.end(); ++it) {
    if (!strcmp((*it)->name->getString(), role)) {
      if (nIndex >= ((*it)->componentList).size()) {
        return OMX_ErrorNoMore;
      }
      OmxNameString *comp = (*it)->componentList[nIndex];
      strncpy(compName, comp->getString(), comp->getLength() + 1);
      return OMX_ErrorNone;
    }
  }
  return OMX_ErrorComponentNotFound;
}

OMX_ERRORTYPE OmxCoreMaster::enumRolesOfComponent(
    OMX_STRING role,
    OMX_STRING compName,
    OMX_U32 nIndex) {
  if (NULL == role) {
    return OMX_ErrorBadParameter;
  }
  for (vector<componentInfo*>::const_iterator it = mComponentInfoList.begin();
       it != mComponentInfoList.end(); ++it) {
    if (!strcmp((*it)->name->getString(), compName)) {
      if (nIndex >= (*it)->roleList.size()) {
        return OMX_ErrorNoMore;
      }
      OmxNameString *role_name = (*it)->roleList[nIndex];
      strncpy(role, role_name->getString(), role_name->getLength() + 1);
      return OMX_ErrorNone;
    }
  }
  return OMX_ErrorComponentNotFound;
}
#endif  // OMX_SPEC_1_2_0_0_SUPPORT

}  // namespace berlin

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#ifdef EXPORT_OMX_CORE_MASTER
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void) {
  berlin::getOmxCoreMaster();
  berlin::gOmxCoreMasterUseCount++;
  return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void) {
  if (--berlin::gOmxCoreMasterUseCount == 0) {
    // AutoMutex _l(gOmxCoreMasterLock);
    if (berlin::getOmxCoreMaster()!= NULL) {
      delete berlin::gOmxCoreMaster;
      berlin::gOmxCoreMaster = NULL;
    }
  }
  return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN OMX_U32 nNameLength,
    OMX_IN OMX_U32 nIndex) {
  return berlin::getOmxCoreMaster()->componentNameEnum(
      cComponentName, nNameLength, nIndex);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN OMX_STRING cComponentName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE* pCallBacks) {
  return berlin::getOmxCoreMaster()->getComponent(
      pHandle, cComponentName, pAppData, pCallBacks);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN OMX_HANDLETYPE hComponent) {
  return berlin::getOmxCoreMaster()->freeComponent(hComponent);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput) {
  return berlin::getOmxCoreMaster()->setupTunnel(
      hOutput, nPortOutput, hInput, nPortInput);
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole(
    OMX_IN      OMX_STRING role,
    OMX_INOUT   OMX_U32 *pNumComps,
    OMX_INOUT   OMX_U8  **compNames) {
  return berlin::getOmxCoreMaster()->getComponentsOfRole(
      role, pNumComps, compNames);
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent(
    OMX_IN      OMX_STRING compName,
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles) {
  return berlin::getOmxCoreMaster()->getRolesOfComponent(
      compName, pNumRoles, roles);
}

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_TeardownTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput) {
  // Teardown output tunnel
  OMX_ERRORTYPE result = OMX_SetupTunnel(hOutput, nPortOutput, NULL, NULL);
  if (OMX_ErrorNone != result)
    return result;
  // Teardown input tunnel
  return OMX_SetupTunnel(NULL, NULL, hInput, nPortInput);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentOfRoleEnum(
    OMX_OUT OMX_STRING compName,
    OMX_IN OMX_STRING role,
    OMX_IN OMX_U32 nIndex) {
  return berlin::getOmxCoreMaster()->enumComponentsOfRole(
      compName, role, nIndex);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_RoleOfComponentEnum(
    OMX_OUT OMX_STRING role,
    OMX_IN OMX_STRING compName,
    OMX_IN OMX_U32 nIndex) {
  return berlin::getOmxCoreMaster()->enumRolesOfComponent(
      role, compName, nIndex);
}
#endif  // OMX_SPEC_1_2_0_0_SUPPORT
#else  // EXPORT_OMX_CORE_MASTER

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void) {
  return berlin::getBerlinCore()->init();
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void) {
  return berlin::getBerlinCore()->deinit();
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN OMX_U32 nNameLength,
    OMX_IN OMX_U32 nIndex) {
  return berlin::getBerlinCore()->componentNameEnum(cComponentName,
                                                    nNameLength, nIndex);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN OMX_STRING cComponentName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE* pCallBacks) {
  return berlin::getBerlinCore()->getHandle(
      pHandle, cComponentName, pAppData, pCallBacks);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN OMX_HANDLETYPE hComponent) {
  return berlin::getBerlinCore()->freeHandle(hComponent);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput) {
  return berlin::getBerlinCore()->setupTunnel(
      hOutput, nPortOutput, hInput, nPortInput);
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole(
    OMX_IN      OMX_STRING role,
    OMX_INOUT   OMX_U32 *pNumComps,
    OMX_INOUT   OMX_U8  **compNames) {
  return berlin::getBerlinCore()->getComponentsOfRole(
      role, pNumComps, compNames);
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent(
    OMX_IN      OMX_STRING compName,
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles) {
  return berlin::getBerlinCore()->getRolesOfComponent(
      compName, pNumRoles, roles);
}

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_TeardownTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput) {
  // Teardown output tunnel
  OMX_ERRORTYPE result = OMX_SetupTunnel(hOutput, nPortOutput, NULL, NULL);
  if (OMX_ErrorNone != result)
    return result;
  // Teardown input tunnel
  return OMX_SetupTunnel(NULL, NULL, hInput, nPortInput);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentOfRoleEnum(
    OMX_OUT OMX_STRING compName,
    OMX_IN OMX_STRING role,
    OMX_IN OMX_U32 nIndex) {
  return berlin::getBerlinCore()->enumComponentsOfRole(compName, role, nIndex);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_RoleOfComponentEnum(
    OMX_OUT OMX_STRING role,
    OMX_IN OMX_STRING compName,
    OMX_IN OMX_U32 nIndex) {
  return berlin::getBerlinCore()->enumRolesOfCompoent(compName, role, nIndex);
}
#endif  // OMX_SPEC_1_2_0_0_SUPPORT
#endif  // EXPORT_OMX_CORE_MASTER

  OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetCoreInterface(
      OMX_OUT void ** ppItf,
      OMX_IN OMX_STRING cExtensionName);

  OMX_API void OMX_APIENTRY OMX_FreeCoreInterface(
      OMX_IN void * pItf);
#ifdef __cplusplus
}
#endif /* __cplusplus */
