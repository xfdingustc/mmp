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

#ifndef BERLIN_DYNAMIC_OMX_CORE_H_
#define BERLIN_DYNAMIC_OMX_CORE_H_

#include <BerlinOmxCore.h>
namespace berlin {
typedef OMX_ERRORTYPE (*InitFunc)();
typedef OMX_ERRORTYPE (*DeinitFunc)();
typedef OMX_ERRORTYPE (*ComponentNameEnumFunc)(
  OMX_STRING, OMX_U32, OMX_U32);

typedef OMX_ERRORTYPE (*GetHandleFunc)(
  OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);

typedef OMX_ERRORTYPE (*FreeHandleFunc)(OMX_HANDLETYPE);

typedef OMX_ERRORTYPE (*GetRolesOfComponentFunc)(
  OMX_STRING, OMX_U32 *, OMX_U8 **);

typedef OMX_ERRORTYPE (*SetupTunnelFunc)(
  OMX_HANDLETYPE, OMX_U32, OMX_HANDLETYPE, OMX_U32);

typedef OMX_ERRORTYPE (*GetComponentsOfRoleFunc)(
  OMX_STRING role,
  OMX_U32 *pNumComps,
  OMX_U8 **compNames);

class DynamicOmxCore : public OmxCore {
public:
  DynamicOmxCore();
  explicit DynamicOmxCore(OMX_STRING coreName);
  virtual ~DynamicOmxCore();
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
  OMX_HANDLETYPE mCoreHandle;
  OMX_BOOL mInited;
  InitFunc mInitFunc;
  DeinitFunc mDeinitFunc;
  ComponentNameEnumFunc mComponentNameEnumFunc;
  GetHandleFunc mGetHandleFunc;
  FreeHandleFunc mFreeHandleFunc;
  GetComponentsOfRoleFunc mGetComponentsOfRoleFunc;
  GetRolesOfComponentFunc mGetRolesOfComponentFunc;
  SetupTunnelFunc mSetupTunnelFunc;
};
} // namespace berlin
#endif // BERLIN_DYNAMIC_OMX_CORE_H_
