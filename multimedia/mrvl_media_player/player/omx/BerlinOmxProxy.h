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

#ifndef Berlin_OMX_PROXY_H_
#define Berlin_OMX_PROXY_H_

extern "C" {
#include <OMX_Core.h>
#include <OMX_CoreExt.h>
#include <OMX_Component.h>
}

#include <list>

using namespace std;

namespace mmp {

class BerlinOmxProxy {
  public:
    virtual ~BerlinOmxProxy();

    OMX_ERRORTYPE makeComponentInstance(
            const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

    OMX_ERRORTYPE destroyComponentInstance(
            OMX_COMPONENTTYPE *component);

    OMX_ERRORTYPE setupTunnel(
            OMX_COMPONENTTYPE *outputComponent,
            OMX_U32 outputPortIndex,
            OMX_COMPONENTTYPE *inputComponent,
            OMX_U32 inputPortIndex);

    const char* findComponentFromRole(const char *componentRole);

    static BerlinOmxProxy* getInstance();


  protected:
    BerlinOmxProxy();


  private:
    typedef OMX_ERRORTYPE (*InitFunc)();
    typedef OMX_ERRORTYPE (*DeinitFunc)();
    typedef OMX_ERRORTYPE (*ComponentNameEnumFunc)(
            OMX_STRING, OMX_U32, OMX_U32);
    typedef OMX_ERRORTYPE (*GetHandleFunc)(
            OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);
    typedef OMX_ERRORTYPE (*FreeHandleFunc)(OMX_HANDLETYPE *);
    typedef OMX_ERRORTYPE (*GetRolesOfComponentFunc)(
            OMX_STRING, OMX_U32 *, OMX_U8 **);
    typedef OMX_ERRORTYPE (*SetupTunnelFunc)(
            OMX_HANDLETYPE, OMX_U32, OMX_HANDLETYPE, OMX_U32);

    // Do not allow copy constructors.
    BerlinOmxProxy(const BerlinOmxProxy &);
    BerlinOmxProxy &operator=(const BerlinOmxProxy &);

    void initHwOmx();
    void initSwOmx();
    void init();
    void deinit();
    void getCompsAndRoles(void* omx_handle);
    void* getOmxLibHandle(const char* compname);

    struct ComponentInfo {
      char     mName[OMX_MAX_STRINGNAME_SIZE];
      OMX_U32  numRoles;
      OMX_U8** mRoles;
      void*    omxLibHandle;
    };

    struct InstantiatedComponents {
      OMX_COMPONENTTYPE* comp_handle;
      void* lib_handle;
    };

    list<void*> mOmxLibHandlesList;
    list<InstantiatedComponents*> mInstantiatedCompsList;
    list<ComponentInfo*> mCompInfoList;

    // Singleton
    static BerlinOmxProxy *sProxy;
};

}

#endif
