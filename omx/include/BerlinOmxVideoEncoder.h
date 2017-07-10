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


#ifndef BERLIN_OMX_VIDEO_ENCODER_H_
#define BERLIN_OMX_VIDEO_ENCODER_H_

#include <BerlinOmxComponentImpl.h>

#ifdef _ANDROID_
using namespace android;
#endif

namespace berlin {

#define OMX_ROLE_VIDEO_ENCODER_H263  "video_encoder.h263"
#define OMX_ROLE_VIDEO_ENCODER_AVC   "video_encoder.avc"
#define OMX_ROLE_VIDEO_ENCODER_MPEG4 "video_encoder.mpeg4"
#define OMX_ROLE_VIDEO_ENCODER_VP8   "video_encoder.vp8"

struct ProfileLevelType;
class OmxVideoEncoder;

class OmxVideoEncoderPort : public OmxAmpPort{
public:
  OmxVideoEncoderPort(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxVideoEncoderPort();
  virtual void updateDomainParameter();
  virtual void updateDomainDefinition();
  OMX_BUFFERHEADERTYPE *mReturnedBuffer;
  OMX_U32 mPushedBdNum;
  OMX_U32 mReturnedBdNum;
  OMX_BOOL mEos;
  OmxVideoEncoder *mEncoder;
};

class OmxVideoEncoderInputPort : public OmxVideoEncoderPort{
public:
  OmxVideoEncoderInputPort(OMX_U32 index);
  virtual ~OmxVideoEncoderInputPort();
  virtual void pushBuffer(OMX_BUFFERHEADERTYPE *buffer);
  virtual OMX_ERRORTYPE getVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param);
  virtual OMX_ERRORTYPE setVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param);
  virtual void setDefinition(OMX_PARAM_PORTDEFINITIONTYPE *definition);
#ifdef _ANDROID_
  OMX_U8 *extractGrallocData(void *data, buffer_handle_t *buffer);
  void releaseGrallocData(buffer_handle_t buffer);
#endif
  OMX_U32 mFrameSize;
  OMX_U32 mWidth;
  OMX_U32 mHeight;
};

class OmxVideoEncoderOutputPort : public OmxVideoEncoderPort{
public:
  OmxVideoEncoderOutputPort(OMX_U32 index);
  virtual ~OmxVideoEncoderOutputPort();
  virtual void pushBuffer(OMX_BUFFERHEADERTYPE *buffer);
  virtual OMX_ERRORTYPE getVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param);
  virtual OMX_ERRORTYPE setVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param);
  OMX_BOOL mIsExtraDataGot;
};

class OmxVideoEncoder : public OmxComponentImpl {
public:
  OmxVideoEncoder();
  OmxVideoEncoder(OMX_STRING name);
  virtual ~OmxVideoEncoder();
  virtual OMX_ERRORTYPE initRole();
  virtual OMX_ERRORTYPE initPort();
  virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
  virtual OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, const OMX_PTR params);
  virtual OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index, const OMX_PTR params);
  virtual OMX_ERRORTYPE getConfig(OMX_INDEXTYPE index, OMX_PTR config);
  virtual OMX_ERRORTYPE setConfig(OMX_INDEXTYPE index, const OMX_PTR config);
  virtual OMX_ERRORTYPE prepare();
  virtual OMX_ERRORTYPE preroll();
  virtual OMX_ERRORTYPE start();
  virtual OMX_ERRORTYPE pause();
  virtual OMX_ERRORTYPE resume();
  virtual OMX_ERRORTYPE stop();
  virtual OMX_ERRORTYPE release();
  virtual OMX_ERRORTYPE flush();
  void clearPort();
  static OMX_ERRORTYPE createComponent(
      OMX_HANDLETYPE* handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE* callBacks);

  static HRESULT bufferCb(
      AMP_COMPONENT component,
      AMP_PORT_IO port_Io,
      UINT32 port_idx,
      AMP_BD_ST *bd,
      void *context);

private:
  OMX_ERRORTYPE createAmpVenc();
  OMX_ERRORTYPE destroyAmpVenc();
  OMX_ERRORTYPE registerBds(OmxVideoEncoderPort *port);
  OMX_ERRORTYPE unregisterBds(OmxVideoEncoderPort *port);
  OMX_ERRORTYPE pushAmpBd(AMP_PORT_IO port, UINT32 portindex, AMP_BD_ST *bd);

public:
  OmxVideoEncoderInputPort *mInputPort;
  OmxVideoEncoderOutputPort *mOutputPort;
  union {
    OMX_VIDEO_PARAM_AVCTYPE avc;
    OMX_VIDEO_PARAM_MPEG4TYPE mpeg4;
    OMX_VIDEO_PARAM_H263TYPE h263;
  } mCodecParam;
  TimeStampRecover mTsRecover;

private:
  AMP_STATE mAmpState;
  OMX_BOOL mEOS;
  OMX_BOOL mOutputEOS;
  OMX_MARKTYPE mMark;
  const ProfileLevelType *mProfileLevelPtr;
  OMX_U32 mProfileLevelNum;
  OMX_U32 mFramerateQ16;
};

} // namespace berlin
#endif //BERLIN_OMX_VIDEO_ENCODER_H_


