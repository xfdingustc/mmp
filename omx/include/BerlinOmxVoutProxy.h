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


#ifndef BERLIN_OMX_VOUT_PROXY_H_
#define BERLIN_OMX_VOUT_PROXY_H_

extern "C" {
#include <amp_event_listener.h>
}

#include <BerlinOmxComponentImpl.h>
#include <BerlinOmxAmpVideoPort.h>
#include <BerlinOmxAmpClockPort.h>

namespace berlin {

class OmxVout {
public:
  AMP_COMPONENT mAmpHandle;
  AMP_STATE mAmpState;
  HANDLE mListener;
  OMX_S32 mVideoPlaneId;

  OmxVout(OMX_S32 plane_id);
  virtual ~OmxVout();
  OMX_ERRORTYPE connectVideoPort(AMP_COMPONENT comp, UINT32 port);
  OMX_ERRORTYPE disconnectVideoPort(AMP_COMPONENT comp, UINT32 port);
  OMX_ERRORTYPE connectClockPort(AMP_COMPONENT comp, UINT32 port);
  OMX_ERRORTYPE disconnectClockPort(AMP_COMPONENT comp, UINT32 port);
  OMX_ERRORTYPE createAmpVout();
  OMX_ERRORTYPE destroyAmpVout();
  HRESULT registerEvent(AMP_EVENT_CODE event, AMP_EVENT_CALLBACK func,
      VOID *userData);
  HRESULT unRegisterEvent(AMP_EVENT_CODE event, AMP_EVENT_CALLBACK func);
  OMX_ERRORTYPE prepare();
  OMX_ERRORTYPE preroll();
  OMX_ERRORTYPE start();
  OMX_ERRORTYPE pause();
  OMX_ERRORTYPE resume();
  OMX_ERRORTYPE stop();
  OMX_ERRORTYPE release();
  OMX_ERRORTYPE flush();
};

class OmxVoutProxy : public OmxComponentImpl {
public:
  OmxVoutProxy();
  virtual ~OmxVoutProxy();
  virtual OMX_ERRORTYPE prepare();
  virtual OMX_ERRORTYPE preroll();
  virtual OMX_ERRORTYPE start();
  virtual OMX_ERRORTYPE pause();
  virtual OMX_ERRORTYPE resume();
  virtual OMX_ERRORTYPE stop();
  virtual OMX_ERRORTYPE release();
  virtual OMX_ERRORTYPE flush();
  virtual OMX_S32 getVideoPlane();
  inline OmxVout * getOmxVout() {return mVout[mVideoPlaneId];};

  static HRESULT eventHandle(HANDLE hListener, AMP_EVENT *pEvent,
      VOID *pUserData);
  OMX_BOOL mIsInputComponent;
  OMX_S32 mVideoPlaneId;
  static OmxVout *mVout[2];
  static OMX_U32 mUserCount[2];
  OMX_BOOL bSendDisplayed;
};
} // namespace berlin
#endif // BERLIN_OMX_VOUT_PROXY_H_
