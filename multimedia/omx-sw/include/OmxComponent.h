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

#ifndef OMX_COMPONENT_H_
#define OMX_COMPONENT_H_

#include <OMX_Component.h>
#include "BerlinOmxCommon.h"

namespace mmp {
class OmxComponent {
public:
  OmxComponent();
  OmxComponent(const OMX_CALLBACKTYPE *callbacks, OMX_PTR appData);
  virtual ~OmxComponent();

  virtual OMX_ERRORTYPE getComponentVersion(
      OMX_STRING componentName,
      OMX_VERSIONTYPE* componentVersion,
      OMX_VERSIONTYPE* specVersion,
      OMX_UUIDTYPE* componentUUID) = 0;

  virtual OMX_ERRORTYPE sendCommand(
      OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR cmdData) = 0;

  virtual OMX_ERRORTYPE getParameter(
      OMX_INDEXTYPE index, OMX_PTR params) = 0;

  virtual OMX_ERRORTYPE setParameter(
      OMX_INDEXTYPE index, const OMX_PTR params) = 0;

  virtual OMX_ERRORTYPE getConfig(
      OMX_INDEXTYPE index, OMX_PTR config) = 0;

  virtual OMX_ERRORTYPE setConfig(
      OMX_INDEXTYPE index, const OMX_PTR config) = 0;

  virtual OMX_ERRORTYPE getExtensionIndex(
      const OMX_STRING name, OMX_INDEXTYPE *index) = 0;

  virtual OMX_ERRORTYPE useBuffer(
      OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_U32 portIndex,
      OMX_PTR appPrivate,
      OMX_U32 size,
      OMX_U8 *buffer) = 0;

  virtual OMX_ERRORTYPE allocateBuffer(
      OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_U32 portIndex,
      OMX_PTR appPrivate,
      OMX_U32 size) = 0;

  virtual OMX_ERRORTYPE freeBuffer(
      OMX_U32 portIndex,
      OMX_BUFFERHEADERTYPE *buffer) = 0;

  virtual OMX_ERRORTYPE useEGLImage(
      OMX_BUFFERHEADERTYPE** bufHdr,
      OMX_U32 portIndex,
      OMX_PTR appPrivate,
      void* eglImage) = 0;

  virtual OMX_ERRORTYPE componentDeInit(
      OMX_HANDLETYPE hComponent) = 0;

  virtual OMX_ERRORTYPE emptyThisBuffer(OMX_BUFFERHEADERTYPE *buffer) = 0;
  virtual OMX_ERRORTYPE fillThisBuffer(OMX_BUFFERHEADERTYPE *buffer) = 0;

  virtual OMX_ERRORTYPE componentRoleEnum(OMX_U8 *role, OMX_U32 index) = 0;

  virtual OMX_ERRORTYPE getState(OMX_STATETYPE *state) = 0;

  virtual OMX_ERRORTYPE componentTunnelRequest(
      OMX_U32 port,
      OMX_HANDLETYPE tunneledComp,
      OMX_U32 tunneledPort,
      OMX_TUNNELSETUPTYPE* tunnelSetup) = 0;

  OMX_ERRORTYPE setCallbacks (
      OMX_CALLBACKTYPE* callbacks,
      OMX_PTR appData) ;
  static OMX_COMPONENTTYPE *makeComponent(OmxComponent *base);

protected:
  void postEvent(OMX_EVENTTYPE event, OMX_U32 param1, OMX_U32 param2);
  void postFillBufferDone(OMX_BUFFERHEADERTYPE *bufHdr);
  void postEmptyBufferDone(OMX_BUFFERHEADERTYPE *bufHdr);
  void postMark(OMX_PTR pMarkData);
  OMX_COMPONENTTYPE* getComponentHandle();

private:
  const OMX_CALLBACKTYPE *mCallbacks;
  OMX_PTR mAppData;
  OMX_COMPONENTTYPE *mComponentHandle;
  void setComponentHandle(OMX_COMPONENTTYPE *handle);
  OmxComponent(const OmxComponent &);
  OmxComponent &operator=(const OmxComponent &);
};
}
#endif  // BERLIN_OMX_COMPONENT_H_
