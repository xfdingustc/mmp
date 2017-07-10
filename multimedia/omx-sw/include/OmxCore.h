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

namespace mmp {

typedef struct ComponentEntry_ {
  char name[OMX_MAX_STRINGNAME_SIZE];
} ComponentEntry;

class OmxCore {
  public:
    static OmxCore* getInstance();
    ~OmxCore();
    OMX_ERRORTYPE init();
    OMX_ERRORTYPE deinit();
    OMX_ERRORTYPE getHandle(
        OMX_HANDLETYPE* handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE* callBacks);
    OMX_ERRORTYPE freeHandle(OMX_HANDLETYPE hComponent);
    OMX_ERRORTYPE setupTunnel(
        OMX_HANDLETYPE hOutput,
        OMX_U32 nPortOutput,
        OMX_HANDLETYPE hInput,
        OMX_U32 nPortInput);

    OMX_ERRORTYPE componentNameEnum(
        OMX_STRING componentName,
        OMX_U32 nameLength,
        OMX_U32 index);
    OMX_ERRORTYPE getComponentsOfRole(
        OMX_STRING role,
        OMX_U32 *pNumComps,
        OMX_U8 **compNames);
    OMX_ERRORTYPE getRolesOfComponent(
        OMX_STRING compName,
        OMX_U32 *pNumRoles,
        OMX_U8 **roles);
    OMX_ERRORTYPE getContentPipe(
        OMX_HANDLETYPE *hPipe,
        OMX_STRING szURI);


  protected:
    OmxCore();


  private:
    vector<ComponentEntry> mComponentVector;
    vector<OMX_HANDLETYPE> mHandleVector;
    OMX_BOOL mInited;

    // Singleton
    static OmxCore *sOmxCore_;
};

}

#endif
