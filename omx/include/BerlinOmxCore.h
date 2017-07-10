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

#ifndef BERLIN_OMX_CORE_H_
#define BERLIN_OMX_CORE_H_

#include <string.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <BerlinOmxCommon.h>

#include <list>
#include <vector>
#include <string>
#include <map>
using namespace std;

namespace berlin {

typedef OMX_ERRORTYPE (*createComponentFunc)(
  OMX_HANDLETYPE* handle,
  OMX_STRING componentName,
  OMX_PTR appData,
  OMX_CALLBACKTYPE* callBacks);

typedef struct ComponentCreateEntry_ {
  createComponentFunc createFunc;
  OMX_HANDLETYPE componentHandle;
} ComponentCreateEntry;

typedef struct ComponentEntry_ {
  char name[OMX_MAX_STRINGNAME_SIZE];
  createComponentFunc createFunc;
  OMX_HANDLETYPE libHandle;
} ComponentEntry;

typedef pair<createComponentFunc, OMX_HANDLETYPE> ComponentCreatePair;
typedef pair<string, ComponentCreateEntry> ComponentPair;
typedef map<string, ComponentCreateEntry> ComponentMap;

class OmxCore {
  public:
    virtual ~OmxCore() {}
    virtual OMX_ERRORTYPE init() = 0;
    virtual OMX_ERRORTYPE deinit() = 0;
    virtual OMX_ERRORTYPE getHandle(
        OMX_HANDLETYPE* handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE* callBacks) = 0;
    virtual OMX_ERRORTYPE freeHandle(OMX_HANDLETYPE hComponent) = 0;
    virtual OMX_ERRORTYPE setupTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput) = 0;

    virtual OMX_ERRORTYPE componentNameEnum(
        OMX_STRING componentName,
        OMX_U32 nameLength,
        OMX_U32 index) = 0;

    virtual OMX_ERRORTYPE getComponentsOfRole(
        OMX_STRING role,
        OMX_U32 *pNumComps,
        OMX_U8 **compNames) = 0;
    virtual OMX_ERRORTYPE getRolesOfComponent(
        OMX_STRING compName,
        OMX_U32 *pNumRoles,
        OMX_U8 **roles) = 0;

    virtual OMX_ERRORTYPE getContentPipe(
        OMX_HANDLETYPE *hPipe,
        OMX_STRING szURI) = 0;

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    virtual OMX_ERRORTYPE teardownTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput) = 0;

    virtual OMX_ERRORTYPE enumComponentOfRole(
        OMX_STRING compName,
        OMX_STRING role,
        OMX_U32 nIndex) = 0;

    virtual OMX_ERRORTYPE enumRoleOfComponent(
        OMX_STRING role,
        OMX_STRING compName,
        OMX_U32 nIndex) = 0;
#endif // OMX_SPEC_1_2_0_0_SUPPORT
};


class BerlinCore : public OmxCore {
  public:
    BerlinCore();
    ~BerlinCore();
    virtual OMX_ERRORTYPE init();
    virtual OMX_ERRORTYPE deinit();
    virtual OMX_ERRORTYPE getHandle(
        OMX_HANDLETYPE* handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE* callBacks);
    virtual OMX_ERRORTYPE freeHandle(OMX_HANDLETYPE hComponent);
    virtual OMX_ERRORTYPE setupTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput);

    virtual OMX_ERRORTYPE componentNameEnum(
        OMX_STRING componentName,
        OMX_U32 nameLength,
        OMX_U32 index);
    virtual OMX_ERRORTYPE getComponentsOfRole(
        OMX_STRING role,
        OMX_U32 *pNumComps,
        OMX_U8 **compNames);
    virtual OMX_ERRORTYPE getRolesOfComponent(
        OMX_STRING compName,
        OMX_U32 *pNumRoles,
        OMX_U8 **roles);

    virtual OMX_ERRORTYPE getContentPipe(
        OMX_HANDLETYPE *hPipe,
        OMX_STRING szURI);

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    virtual OMX_ERRORTYPE teardownTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput);

    virtual OMX_ERRORTYPE enumComponentOfRole(
        OMX_STRING compName,
        OMX_STRING role,
        OMX_U32 nIndex);

    virtual OMX_ERRORTYPE enumRoleOfComponent(
        OMX_STRING role,
        OMX_STRING compName,
        OMX_U32 nIndex);
#endif // OMX_SPEC_1_2_0_0_SUPPORT

  private:
    // ComponentMap mComponentMap;
    vector<ComponentEntry> mComponentVector;
    vector<OMX_HANDLETYPE> mHandleVector;
    OMX_BOOL mInited;
};

class OmxNameString {
  public:
    OmxNameString():
      mData(NULL),
      mLength(0) {
    }
    explicit OmxNameString(OMX_STRING in) {
      mLength = strlen(in);
      mData = new char[mLength + 1];
      strncpy(mData, in, mLength + 1);
    }
    ~OmxNameString() {
      delete mData;
    }
    inline OMX_STRING getString() {return mData;}
    inline OMX_U32 getLength() {return mLength;}
  private:
    OMX_STRING mData;
    OMX_U32 mLength;
};

typedef std::vector<OmxNameString *> NameList;

typedef struct componentInfo {
  OmxNameString *name;
  NameList roleList;
} componentInfo;
typedef std::vector<componentInfo*> Component_List_Type;

typedef struct roleInfo {
  OmxNameString *name;
  NameList componentList;
} roleInfo;
typedef std::vector<roleInfo*> Role_List_Type;

struct CompHandleInfo {
  CompHandleInfo(OMX_HANDLETYPE handle, OmxCore *core) {
    mHandle = handle;
    mCore = core;
  }
  OMX_HANDLETYPE mHandle;
  OmxCore *mCore;
};

class OmxCoreMaster {
  public:
    OmxCoreMaster();
    virtual ~OmxCoreMaster();

    virtual OMX_ERRORTYPE getComponent(
        OMX_HANDLETYPE* handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE* callBacks);

    virtual OMX_ERRORTYPE freeComponent(OMX_HANDLETYPE hComponent);
    virtual OMX_ERRORTYPE setupTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput);

    virtual OMX_ERRORTYPE componentNameEnum(
        OMX_STRING componentName,
        OMX_U32 nameLength,
        OMX_U32 index);

    virtual OMX_ERRORTYPE getComponentsOfRole(
         OMX_STRING role,
         OMX_U32 *pNumComps,
         OMX_U8 **compNames);
     virtual OMX_ERRORTYPE getRolesOfComponent(
         OMX_STRING compName,
         OMX_U32 *pNumRoles,
         OMX_U8 **roles);

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    virtual OMX_ERRORTYPE teardownTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput);

    virtual OMX_ERRORTYPE enumComponentsOfRole(
        OMX_STRING compName,
        OMX_STRING role,
        OMX_U32 nIndex);

    virtual OMX_ERRORTYPE enumRolesOfComponent(
        OMX_STRING role,
        OMX_STRING compName,
        OMX_U32 nIndex);
#endif // OMX_SPEC_1_2_0_0_SUPPORT

  private:
    OMX_ERRORTYPE loadCore(OMX_STRING coreName, OMX_HANDLETYPE* coreHandle);
    OMX_ERRORTYPE loadCore(OMX_HANDLETYPE* coreHandle);
    OMX_ERRORTYPE unloadCore(OMX_HANDLETYPE coreHandle);
    OMX_ERRORTYPE recordComponentAndRole(OmxCore *pcore);
    OMX_ERRORTYPE dumpComponentList();
    OMX_ERRORTYPE dumpRoleList();

    OMX_U32 mComponentNum;
    OMX_U32 mRoleNum;
    vector<OmxCore *> mCoreList;
    vector<CompHandleInfo *> mCompHandleList;
    vector<componentInfo *> mComponentInfoList;
    vector<roleInfo *> mRoleInfoList;
  };
OmxCoreMaster* getOmxCoreMaster();
} // namespace berlin

#endif // BERLIN_OMX_CORE_H_
