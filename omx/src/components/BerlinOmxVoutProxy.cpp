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

#define LOG_TAG "BerlinOmxVout"
#include <BerlinOmxVoutProxy.h>

#define VOUT_TRACE(fmt,...) do { \
  ALOGD("%s() line %d: mVideoPlaneId %d " fmt, __FUNCTION__, __LINE__, \
      mVideoPlaneId, ##__VA_ARGS__); \
} while(0)

namespace berlin {
OMX_U32 OmxVoutProxy::mUserCount[2] = {0, 0};
OmxVout * OmxVoutProxy::mVout[2] = {NULL, NULL};

OmxVout::OmxVout(OMX_S32 plane_id) :
  mVideoPlaneId(plane_id) {
  mAmpHandle = NULL;
  mAmpState = AMP_LOADED;
  mListener = NULL;
}

OmxVout::~OmxVout() {
}

OMX_ERRORTYPE OmxVout::createAmpVout() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_FACTORY factory;
  err = AMP_GetFactory(&factory);
  CHECKAMPERR(err);
  AMP_RPC(err, AMP_FACTORY_CreateComponent, factory, AMP_COMPONENT_VOUT,
      1, &mAmpHandle);
  CHECKAMPERR(err);
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::destroyAmpVout() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  if (mAmpHandle) {
    AMP_RPC(err, AMP_VOUT_Destroy, mAmpHandle);
    CHECKAMPERR(err);
    AMP_FACTORY_Release(mAmpHandle);
    mAmpHandle = NULL;
  }
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::connectVideoPort(AMP_COMPONENT comp, UINT32 port) {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  err = AMP_ConnectComp(comp, port, mAmpHandle, 0);
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::disconnectVideoPort(AMP_COMPONENT comp, UINT32 port) {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  err = AMP_DisconnectComp(comp, port, mAmpHandle, 0);
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::connectClockPort(AMP_COMPONENT comp, UINT32 port) {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  OMX_LOGD("comp %p, port %d, connect to clock port", comp, port);
  err = AMP_ConnectComp(comp, port, mAmpHandle, 1);
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::disconnectClockPort(AMP_COMPONENT comp, UINT32 port) {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  err = AMP_DisconnectComp(comp, port, mAmpHandle, 1);
  return static_cast<OMX_ERRORTYPE>(err);
}

HRESULT OmxVout::registerEvent(AMP_EVENT_CODE event, AMP_EVENT_CALLBACK func,
    VOID *userData) {
  HRESULT err = SUCCESS;
  err = AMP_Event_RegisterCallback(mListener, event, func, userData);
  if (!err) {
    AMP_RPC(err, AMP_VOUT_RegisterNotify, mAmpHandle,
        AMP_Event_GetServiceID(mListener), event);
  }
  return err;
}

HRESULT OmxVout::unRegisterEvent(AMP_EVENT_CODE event,
    AMP_EVENT_CALLBACK func) {
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VOUT_UnregisterNotify, mAmpHandle,
      AMP_Event_GetServiceID(mListener), event);
  if (!err) {
    err = AMP_Event_UnregisterCallback(mListener, event, func);
  }
  return err;
}

OMX_ERRORTYPE OmxVout::prepare() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  err = createAmpVout();
  CHECKAMPERR(err);
  AMP_COMPONENT_CONFIG config;
  kdMemset(&config, 0 ,sizeof(config));
  config._d = AMP_COMPONENT_VOUT;
  config._u.pVOUT.uiInputPortNum = 2;
  config._u.pVOUT.uiOutputPortNum = 0;
  if (mVideoPlaneId < 0) {
    OMX_LOGW("Invalid video plane id %d, using 0 by default", mVideoPlaneId);
    mVideoPlaneId = 0;
  }
  config._u.pVOUT.uiPlaneID = mVideoPlaneId;
  AMP_RPC(err, AMP_VOUT_Open, mAmpHandle, &config);

#ifdef KEEP_LAST_FRAME
  // Keep last frame after flush AMP VOUT.
  // TODO: make it an option to OMX IL's users.
  AMP_VOUT_LastFrameAction last_frame_action = AMP_VOUT_REPEATLASTFRAME;
  AMP_RPC(err, AMP_VOUT_SetLastFrameMode, mAmpHandle, last_frame_action);
  OMX_LOGD("Set AMP_VOUT_REPEATLASTFRAME to VOUT to keep last frame.");
#endif

  mListener = AMP_Event_CreateListener(16, 0);
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::release() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  if (mListener) {
    AMP_Event_DestroyListener(mListener);
    mListener = NULL;
  }
  AMP_RPC(err, AMP_VOUT_Close, mAmpHandle);
  CHECKAMPERR(err);
  err = destroyAmpVout();
  CHECKAMPERR(err);
  VOUT_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::preroll() {
  VOUT_TRACE("Enter");
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxVout::pause() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VOUT_SetState, mAmpHandle, AMP_PAUSED);
  CHECKAMPERR(err);
  mAmpState = AMP_PAUSED;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::resume() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VOUT_SetState, mAmpHandle, AMP_EXECUTING);
  CHECKAMPERR(err);
  mAmpState = AMP_EXECUTING;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::start() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VOUT_SetState, mAmpHandle, AMP_EXECUTING);
  CHECKAMPERR(err);
  mAmpState = AMP_EXECUTING;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::stop() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VOUT_SetState, mAmpHandle, AMP_IDLE);
  CHECKAMPERR(err);
  mAmpState = AMP_IDLE;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVout::flush() {
  VOUT_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VOUT_SetState, mAmpHandle, AMP_IDLE);
  CHECKAMPERR(err);
  AMP_RPC(err, AMP_VOUT_SetState, mAmpHandle, mAmpState);
  VOUT_TRACE("EXIT");
  return static_cast<OMX_ERRORTYPE>(err);
}

#undef LOG_TAG
#define LOG_TAG "BerlinOmxVoutProxy"

HRESULT OmxVoutProxy::eventHandle(HANDLE hListener,
    AMP_EVENT *pEvent, VOID *pUserData) {
  if (pEvent) {
    OMX_LOGV("vout received event %x, pUserData = %p\n",
        pEvent->stEventHead.eEventCode, pUserData);

    OmxVoutProxy *pComp = static_cast<OmxVoutProxy *>(pUserData);
    if (pEvent->stEventHead.eEventCode == AMP_EVENT_API_VOUT_CALLBACK) {
      OMX_LOGD("%s Receive EOS from video render", pComp->mName);
      pComp->postEvent(OMX_EventBufferFlag, kVideoPortStartNumber,
          OMX_BUFFERFLAG_EOS);
      pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoPresentChanged),
          OMX_VIDEO_PRESENCE_FALSE, 0);
    } else if (pEvent->stEventHead.eEventCode ==
        AMP_EVENT_API_VOUT_CALLBACK_FRAME_UPDATE) {
      UINT32 *payload = static_cast<UINT32 *>(AMP_EVENT_PAYLOAD_PTR(pEvent));
      AMP_VOUT_DISP_FRAME_INFO *AmpDisplayInfo =
          reinterpret_cast<AMP_VOUT_DISP_FRAME_INFO *>(payload);
      OMX_LOGV("vout frame pts high 0x%x low 0x%x displayed %d", AmpDisplayInfo->pts_h,
          AmpDisplayInfo->pts_l, AmpDisplayInfo->displayed);
      if (pComp->bSendDisplayed == OMX_FALSE &&
          AmpDisplayInfo->displayed == AMP_VOUT_FRAME_DISPLAYED) {
        OMX_LOGI("Frst frame displayed");
#ifdef VOUT_SEND_DISPLAYED
        OMX_LOGI("Post event OMX_EventDISPLAYED");
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventDISPLAYED),
            AmpDisplayInfo->pts_h, AmpDisplayInfo->pts_l);
#endif
        pComp->bSendDisplayed = OMX_TRUE;
      }
    }
  }
  return SUCCESS;
}

OmxVoutProxy::OmxVoutProxy() {
  mIsInputComponent = OMX_FALSE;
  mVideoPlaneId = -1;
}

OmxVoutProxy::~OmxVoutProxy() {
}

OMX_S32 OmxVoutProxy::getVideoPlane() {
  return -1;
}

OMX_ERRORTYPE OmxVoutProxy::prepare() {
  VOUT_TRACE("%s", mName);
  bSendDisplayed = OMX_FALSE;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mVideoPlaneId = getVideoPlane();
  if (mVideoPlaneId != 0 && mVideoPlaneId != 1) {
    return OMX_ErrorUndefined;
  }
  if (mUserCount[mVideoPlaneId]++ == 0) {
    mVout[mVideoPlaneId] = new OmxVout(mVideoPlaneId);
    err = getOmxVout()->prepare();
  }
  if (OMX_ErrorNone == err) {
    mAmpHandle = mVout[mVideoPlaneId]->mAmpHandle;
  }
  if (NULL != strstr(mName, "iv_processor")) {
    OmxPortImpl *port = getPort(kVideoPortStartNumber);
    if (port && port->isTunneled()) {
      OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(port->getTunnelComponent());
      AMP_COMPONENT amp_tunnel = static_cast<OmxComponentImpl *>(
          omx_tunnel->pComponentPrivate)->getAmpHandle();
      getOmxVout()->connectVideoPort(amp_tunnel, 0);
    }
  }
  if (NULL != strstr(mName, "video_scheduler")) {
    OmxPortImpl *port = getPort(kClockPortStartNumber);
    if (port && port->isTunneled()) {
      OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(port->getTunnelComponent());
      AMP_COMPONENT amp_tunnel = static_cast<OmxComponentImpl *>(
          omx_tunnel->pComponentPrivate)->getAmpHandle();
      OMX_LOGD("%s Clock %p", mName, amp_tunnel);
      // With audio passthru, audio use the first 3 clock port, video use the last one.
      getOmxVout()->connectClockPort(amp_tunnel, 3);
    }
  }
  if (NULL != strstr(mName, "iv_renderer")) {
    getOmxVout()->registerEvent(AMP_EVENT_API_VOUT_CALLBACK, eventHandle, this);
    getOmxVout()->registerEvent(AMP_EVENT_API_VOUT_CALLBACK_FRAME_UPDATE,
        eventHandle, this);
  }
  return err;
}

OMX_ERRORTYPE OmxVoutProxy::release() {
  VOUT_TRACE("%s", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (NULL != strstr(mName, "iv_processor")) {
    OmxPortImpl *port = getPort(kVideoPortStartNumber);
    if (port && port->isTunneled()) {
      OMX_HANDLETYPE omx_handle = port->getTunnelComponent();
      if (NULL != omx_handle) {
        OmxComponentImpl *omx_vdec = static_cast<OmxComponentImpl *>(reinterpret_cast<
        OMX_COMPONENTTYPE *>(omx_handle)->pComponentPrivate);
        if (NULL != omx_vdec) {
          AMP_COMPONENT amp_vdec = omx_vdec->getAmpHandle();
          if (NULL != amp_vdec) {
            err = getOmxVout()->disconnectVideoPort(amp_vdec, 0);
            CHECKAMPERRLOG(err, "disconnect video decoder with VOUT input port");
            OMX_LOGD("disconnect vdec and vout in vout component.");
          } else {
            OMX_LOGD("amp vdec is already released.");
          }
        } else {
          OMX_LOGD("omx vdec component is already released.");
        }
      } else {
        OMX_LOGD("omx vdec handle is already released.");
      }
    }
  }
#if 0
  if (NULL != strstr(mName, "video_scheduler")) {
    OmxPortImpl *port = getPort(kClockPortStartNumber);
    if (port->isTunneled()) {
      OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(port->getTunnelComp());
      AMP_COMPONENT amp_tunnel = static_cast<OmxComponentImpl *>(
          omx_tunnel->pComponentPrivate)->getAmpHandle();
      getOmxVout()->disconnectClockPort(amp_tunnel, 0);
    }
  }
#endif
  if (NULL != strstr(mName, "iv_renderer")) {
    getOmxVout()->unRegisterEvent(AMP_EVENT_API_VOUT_CALLBACK, eventHandle);
    getOmxVout()->unRegisterEvent(AMP_EVENT_API_VOUT_CALLBACK_FRAME_UPDATE,
        eventHandle);
  }

  if (--mUserCount[mVideoPlaneId] == 0) {
    err = mVout[mVideoPlaneId]->release();
    delete mVout[mVideoPlaneId];
    mVout[mVideoPlaneId] = NULL;
  }
  mAmpHandle = NULL;
  return err;
}

OMX_ERRORTYPE OmxVoutProxy::preroll() {
  VOUT_TRACE("%s", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mIsInputComponent) {
    err = getOmxVout()->preroll();
  }
  return err;
}

OMX_ERRORTYPE OmxVoutProxy::pause() {
  VOUT_TRACE("%s", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mIsInputComponent) {
    err = getOmxVout()->pause();
  }
  return err;
}
OMX_ERRORTYPE OmxVoutProxy::resume() {
  VOUT_TRACE("%s", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mIsInputComponent) {
    err = getOmxVout()->resume();
  }
  return err;
}

OMX_ERRORTYPE OmxVoutProxy::start() {
  VOUT_TRACE("%s", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mIsInputComponent) {
    err = getOmxVout()->start();
  }
  return err;
}

OMX_ERRORTYPE OmxVoutProxy::stop() {
  VOUT_TRACE("%s", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mIsInputComponent) {
    err = getOmxVout()->stop();
  }
  return err;
}

OMX_ERRORTYPE OmxVoutProxy::flush() {
  VOUT_TRACE("%s ENTER", mName);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  bSendDisplayed = OMX_FALSE;
  if (mIsInputComponent) {
    err = getOmxVout()->flush();
  }
  VOUT_TRACE("%s EXIT", mName);
  return err;
}

}  // namespace berlin
