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

#include <dlfcn.h>  // for dlopen, dlsym, ...

#include "MHALOmxUtils.h"
#include "BerlinOmxProxy.h"

#undef  LOG_TAG
#define LOG_TAG "BerlinOmxProxy"

namespace mmp {

// TODO: add more OMX Cores here.
static char* OmxCores[] = {
  "libBerlinCore.so",
};

BerlinOmxProxy* BerlinOmxProxy::sProxy = NULL;

BerlinOmxProxy* BerlinOmxProxy::getInstance() {
  if (NULL == sProxy) {
    sProxy = new BerlinOmxProxy();
  }
  return sProxy;
}

BerlinOmxProxy::BerlinOmxProxy()
  : mCompInfoList(NULL) {
  OMX_LOGD("ENTER");
  init();
  OMX_LOGD("EXIT");
}

BerlinOmxProxy::~BerlinOmxProxy() {
  OMX_LOGD("ENTER");
  deinit();
  OMX_LOGD("EXIT");
}

void BerlinOmxProxy::getCompsAndRoles(void* omx_handle) {
  if (omx_handle == NULL) {
    OMX_LOGE("Handle is NULL!");
    return;
  }
  InitFunc initFunc = (InitFunc)dlsym(omx_handle, "OMX_Init");
  if (!initFunc) {
    return;
  }
  // Init the OMX Core.
  (*initFunc)();

  ComponentNameEnumFunc componentNameEnumFunc =
      (ComponentNameEnumFunc)dlsym(omx_handle, "OMX_ComponentNameEnum");
  GetRolesOfComponentFunc getRolesOfComponentHandleFunc =
      (GetRolesOfComponentFunc)dlsym(omx_handle, "OMX_GetRolesOfComponent");
  if (!componentNameEnumFunc || !getRolesOfComponentHandleFunc) {
    return;
  }

  // Load the comps, roles in this OMX core.
  OMX_U32 index = 0;
  char componentName[OMX_MAX_STRINGNAME_SIZE] = {0x00};
  while ((*componentNameEnumFunc)(
      componentName, sizeof(componentName), index) == OMX_ErrorNone) {
    // A new component is found.
    ComponentInfo *node = new ComponentInfo;
    if (!node) {
      OMX_LOGE("Out of memory!");
      break;
    }
    memset(node, 0x00, sizeof(ComponentInfo));
    strncpy(node->mName, componentName, sizeof(componentName));

    // 1/2 Retrieve numRoles only.
    OMX_U32 numRoles;
    OMX_ERRORTYPE err = (*getRolesOfComponentHandleFunc)(
        const_cast<OMX_STRING>(componentName), &numRoles, NULL);
    if (err != OMX_ErrorNone) {
      OMX_LOGE("Failed to get roles of component %s with error %d!", componentName, err);
      break;
    }
    OMX_LOGV("componentName %s has %d roles", componentName, numRoles);
    node->numRoles = numRoles;
    if (node->numRoles > 0) {
      // 2/2 Retrieve roles.
      OMX_U8 **array = new OMX_U8 *[numRoles];
      if (!array) {
        OMX_LOGE("Out of memory!");
        break;
      }
      for (OMX_U32 i = 0; i < numRoles; ++i) {
        array[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
        if(!array[i]) {
          OMX_LOGE("Out of memory!");
          break;
        }
      }

      // Put roles in the array.
      OMX_U32 numRoles2 = numRoles;
      err = (*getRolesOfComponentHandleFunc)(
          const_cast<OMX_STRING>(componentName), &numRoles2, array);
      if (err != OMX_ErrorNone) {
        OMX_LOGE("Failed to get roles of component %s with error %d!", componentName, err);
        break;
      }
      if (numRoles != numRoles2) {
        OMX_LOGE("Failed to get number of roles in component %s!", componentName);
        break;
      }
      node->mRoles = array;
      node->omxLibHandle = omx_handle;
      for (OMX_U32 idx = 0; idx < node->numRoles; idx++) {
        OMX_LOGD("Found component %s with role(%d/%d) %s",
            node->mName, idx + 1, node->numRoles, node->mRoles[idx]);
      }
    }

    // Add the newly found component to list.
    mCompInfoList.push_back(node);

    // Move to get the next component.
    ++index;
  }

}

void BerlinOmxProxy::init() {
  OMX_U32 numCores = (int) (sizeof(OmxCores) / sizeof(OmxCores[0]));
  OMX_LOGD("Loading %d omx library.", numCores);
  for (OMX_U32 i = 0; i < numCores; i++) {
    void* lib_handle = dlopen(OmxCores[i], RTLD_NOW);
    if (!lib_handle) {
      OMX_LOGE("Error loading omx library %s", OmxCores[i]);
      break;
    }
    mOmxLibHandlesList.push_back(lib_handle);
    getCompsAndRoles(lib_handle);
  }
}

void BerlinOmxProxy::deinit() {
  OMX_LOGD("Unloading vendor omx library.");
  for(list<void*>::iterator it = mOmxLibHandlesList.begin(); it != mOmxLibHandlesList.end();) {
    DeinitFunc deinitFunc = (DeinitFunc)dlsym((*it), "OMX_Deinit");
    if (deinitFunc) {
      (*deinitFunc)();
    }
    it = mOmxLibHandlesList.erase(it);
  }

  OMX_LOGD("Release component-role list.");
  for(list<ComponentInfo*>::iterator it = mCompInfoList.begin(); it != mCompInfoList.end();) {
    // Free this node.
    for (OMX_U32 i = 0; i < (*it)->numRoles; ++i) {
      delete [] (*it)->mRoles[i];
    }
    delete (*it);
    it = mCompInfoList.erase(it);
  }
  OMX_LOGD("EXIT");
}

OMX_ERRORTYPE BerlinOmxProxy::makeComponentInstance(
    const char *name,
    const OMX_CALLBACKTYPE *callbacks,
    OMX_PTR appData,
    OMX_COMPONENTTYPE **component) {
  void* lib_handle = NULL;
  list<ComponentInfo*>::iterator it;
  for(it = mCompInfoList.begin(); it != mCompInfoList.end(); ++it) {
    if (!strcasecmp(name, (*it)->mName)) {
      // found the component
      lib_handle = (*it)->omxLibHandle;
    }
  }
  if (!lib_handle) {
    return OMX_ErrorUndefined;
  }
  GetHandleFunc getHandleFunc = (GetHandleFunc)dlsym(lib_handle, "OMX_GetHandle");
  if (!getHandleFunc) {
    return OMX_ErrorUndefined;
  }

  OMX_ERRORTYPE err = (*getHandleFunc)(
      reinterpret_cast<OMX_HANDLETYPE *>(component),
      const_cast<char *>(name),
      appData, const_cast<OMX_CALLBACKTYPE *>(callbacks));
  if (OMX_ErrorNone == err) {
    InstantiatedComponents* item = new InstantiatedComponents;
    item->comp_handle = *component;
    item->lib_handle = lib_handle;
    mInstantiatedCompsList.push_back(item);
    OMX_LOGD("Component %s is created, there are %d comps created",
        name, mInstantiatedCompsList.size());
  }
  return err;
}

OMX_ERRORTYPE BerlinOmxProxy::destroyComponentInstance(
    OMX_COMPONENTTYPE *component) {
  void* lib_handle = NULL;
  list<InstantiatedComponents*>::iterator it;
  for(it = mInstantiatedCompsList.begin(); it != mInstantiatedCompsList.end(); ++it) {
    if (component == (*it)->comp_handle) {
      lib_handle = (*it)->lib_handle;
      break;
    }
  }
  if (!lib_handle) {
    return OMX_ErrorUndefined;
  }

  FreeHandleFunc freeHandleFunc = (FreeHandleFunc)dlsym(lib_handle, "OMX_FreeHandle");
  if (!freeHandleFunc) {
    return OMX_ErrorUndefined;
  }

  delete (*it);
  mInstantiatedCompsList.erase(it);
  OMX_LOGD("Destroy one components, there are %d comps alive.",
      mInstantiatedCompsList.size());
  return (*freeHandleFunc)(reinterpret_cast<OMX_HANDLETYPE *>(component));
}

OMX_ERRORTYPE BerlinOmxProxy::setupTunnel(
    OMX_COMPONENTTYPE *outputComponent, OMX_U32 outputPortIndex,
    OMX_COMPONENTTYPE *inputComponent, OMX_U32 inputPortIndex) {
  OMX_LOGD("%s %d", __FUNCTION__, __LINE__);
  if (!outputComponent || !inputComponent) {
    OMX_LOGE("Component not exist!!!");
    return OMX_ErrorComponentNotFound;
  }

  void* output_handle = NULL;
  void* input_handle = NULL;
  list<InstantiatedComponents*>::iterator it;
  for(it = mInstantiatedCompsList.begin(); it != mInstantiatedCompsList.end(); ++it) {
    if (outputComponent == (*it)->comp_handle) {
      output_handle = (*it)->lib_handle;
    } else if (inputComponent == (*it)->comp_handle) {
      input_handle = (*it)->lib_handle;
    }
    if (output_handle && input_handle) {
      break;
    }
  }
  if (!input_handle || !output_handle) {
    return OMX_ErrorUndefined;
  }
  if (input_handle != output_handle) {
    OMX_LOGE("Can't set up tunnel for comps in different OMX Core!!!");
    return OMX_ErrorComponentNotFound;
  }

  SetupTunnelFunc setupTunnelFunc = (SetupTunnelFunc)dlsym(input_handle, "OMX_SetupTunnel");
  if (!setupTunnelFunc) {
    OMX_LOGE("Set up tunnel is not supported in this OMX Core!!!");
    return OMX_ErrorComponentNotFound;
  }

  return (*setupTunnelFunc)(
      outputComponent, outputPortIndex, inputComponent, inputPortIndex);
}

const char* BerlinOmxProxy::findComponentFromRole(const char *componentRole) {
  list<ComponentInfo*>::iterator it;
  for(it = mCompInfoList.begin(); it != mCompInfoList.end(); ++it) {
    for(OMX_U32 i = 0; i < (*it)->numRoles; i++) {
      if (!strcasecmp(componentRole, (char *)((*it)->mRoles[i]))) {
        // found the component
        OMX_LOGD("Found component %s", (*it)->mName);
        return (*it)->mName;
      }
    }
  }
  OMX_LOGD("Component not found!");
  return NULL;
}

}
