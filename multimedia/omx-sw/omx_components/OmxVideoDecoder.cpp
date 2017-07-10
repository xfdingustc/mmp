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

#define LOG_TAG "OmxVideoDecoder"

#include <OmxVideoDecoder.h>

#ifdef _ANDROID_
#include <HardwareAPI.h>
#include <GraphicBuffer.h>
#endif

#define HAL_PIXEL_FORMAT_UYVY 0x100

using namespace android;
namespace mmp {

#define OMX_ROLE_VIDEO_DECODER_H263  "video_decoder.h263"
#define OMX_ROLE_VIDEO_DECODER_AVC   "video_decoder.avc"
#define OMX_ROLE_VIDEO_DECODER_MPEG2 "video_decoder.mpeg2"
#define OMX_ROLE_VIDEO_DECODER_MPEG4 "video_decoder.mpeg4"
#define OMX_ROLE_VIDEO_DECODER_RV    "video_decoder.rv"
#define OMX_ROLE_VIDEO_DECODER_WMV   "video_decoder.wmv"
#define OMX_ROLE_VIDEO_DECODER_VC1   "video_decoder.vc1"
#define OMX_ROLE_VIDEO_DECODER_MJPEG   "video_decoder.mjpeg"
#ifdef _ANDROID_
#define OMX_ROLE_VIDEO_DECODER_VPX   "video_decoder.vpx"
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
#define OMX_ROLE_VIDEO_DECODER_VP8   "video_decoder.vp8"
#endif

#ifdef _ANDROID_
static OMX_U32 getNativeBufferSize(int hal_color, OMX_U32 width, OMX_U32 height) {
  OMX_U32 stride = width;
  OMX_U32 size = stride * height;
  switch (hal_color) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
      stride *= 4;
      size *= 4;
      break;
    case HAL_PIXEL_FORMAT_RGB_888:
      stride *= 3;
      size *= 3;
      break;
    case HAL_PIXEL_FORMAT_RGB_565:
      stride *= 2;
      size *= 2;
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_UYVY:
      stride = (stride + 15) & (~15);
      height = (height + 15) & (~15);
      size = stride * height * 2;
      break;
    case HAL_PIXEL_FORMAT_YV12:
      // YCrCb 4:2:0 Planar
      // This format assumes
      // - a horizontal stride multiple of 16 pixels
      // - a vertical stride equal to the height
      // That's to say, it needs 16 bytes alignment for u stride and v stride
      OMX_LOGD("Before align to 32, stride = %d, height = %d", stride, height);
      stride = (stride + 31) & (~31);
      height = (height + 31) & (~31);
      OMX_LOGD("After align to 32, stride = %d, height = %d", stride, height);
      size = stride * height * 3 / 2;
      break;
    default:
      break;
  }
  return size;
}

int getAlignedStride(int hal_color, OMX_U32 width) {
  switch(hal_color) {
    case HAL_PIXEL_FORMAT_YV12:
      return (width + 15) & (~15);
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_UYVY:
      return (width + 15) & (~15);
    default:
      return width;
  }
}

int getAlignedHeight(int hal_color, OMX_U32 height) {
  switch(hal_color) {
    case HAL_PIXEL_FORMAT_YV12:
      return (height + 15) & (~15);
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_UYVY:
      return (height + 15) & (~15);
    default:
      return height;
  }
}
#endif

OmxVideoDecoder::OmxVideoDecoder(OMX_STRING name) :
    OmxComponentImpl(name),
    mProfile(1),
    mInited(OMX_FALSE),
    mFFmpegDecoder(NULL),
    mCodecSpecficDataSent(OMX_FALSE),
    mEOS(OMX_FALSE),
    mHasDecodedFrame(0) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
  mMark.hMarkTargetComponent = NULL;
  mMark.pMarkData = NULL;

  mFFmpegDecoder = new FFMpegVideoDecoder();
}

OmxVideoDecoder::~OmxVideoDecoder() {
  if (mFFmpegDecoder) {
    delete mFFmpegDecoder;
    mFFmpegDecoder = NULL;
  }
}

OMX_ERRORTYPE OmxVideoDecoder::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_H263, 18))
    addRole(OMX_ROLE_VIDEO_DECODER_H263);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_AVC, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_AVC);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MPEG2, 19))
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG2);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MPEG4, 19))
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG4);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_RV, 16))
    addRole(OMX_ROLE_VIDEO_DECODER_RV);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_WMV, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_WMV);
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VC1, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_VC1);
#endif
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MJPEG, 19))
    addRole(OMX_ROLE_VIDEO_DECODER_MJPEG);
#ifdef _ANDROID_
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VPX, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_VPX);
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VP8, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_VP8);
#endif
  else if (!strncmp(role_name, "video_decoder", 13)) {
    addRole(OMX_ROLE_VIDEO_DECODER_AVC);
    addRole(OMX_ROLE_VIDEO_DECODER_H263);
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG4);
    addRole(OMX_ROLE_VIDEO_DECODER_RV);
    addRole(OMX_ROLE_VIDEO_DECODER_WMV);
#ifdef _ANDROID_
    addRole(OMX_ROLE_VIDEO_DECODER_VPX);
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    addRole(OMX_ROLE_VIDEO_DECODER_VP8);
    addRole(OMX_ROLE_VIDEO_DECODER_VC1);
#endif
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}
OMX_ERRORTYPE OmxVideoDecoder::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_AVC, 17)) {
    mInputPort = new  OmxAvcPort(kVideoPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_H263, 18)) {
    mInputPort = new  OmxH263Port(kVideoPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_MPEG4, 19)) {
    mInputPort = new  OmxMpeg4Port(kVideoPortStartNumber, OMX_DirInput);
#ifdef _ANDROID_
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_VPX, 17)) {
    mInputPort = new  OmxVPXPort(kVideoPortStartNumber, OMX_DirInput);
#endif
  } else {
    mInputPort = new OmxVideoPort(kVideoPortStartNumber, OMX_DirInput);
  }
  addPort(mInputPort);
  mOutputPort = new OmxVideoPort(kVideoPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);
  mUseNativeBuffers = false;
  return err;
}

OMX_ERRORTYPE OmxVideoDecoder::getParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamVideoPortFormat: {
      OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param =
          reinterpret_cast<OMX_VIDEO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(video_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(video_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (mInputPort->getPortIndex() == video_param->nPortIndex) {
        if (video_param->nIndex >= NELM(kFormatSupport)) {
          err = OMX_ErrorNoMore;
          break;
        }
        const VideoFormatType *v_fmt = &kFormatSupport[video_param->nIndex];
        video_param->eCompressionFormat = v_fmt->eCompressionFormat;
        video_param->eColorFormat = v_fmt->eColorFormat;
        video_param->xFramerate = v_fmt->xFramerate;
      } else {
        if (video_param->nIndex >= 1) {
          err = OMX_ErrorNoMore;
          break;
        }
        OMX_VIDEO_PORTDEFINITIONTYPE &v_def = mOutputPort->getVideoDefinition();
        OMX_LOGD("on port %d video resolution is (%d x %d)",
            video_param->nPortIndex, v_def.nFrameWidth, v_def.nFrameHeight);
        video_param->eCompressionFormat = v_def.eCompressionFormat;
        video_param->eColorFormat = v_def.eColorFormat;
        video_param->xFramerate = v_def.xFramerate;
      }
      break;
    }
    case OMX_IndexParamVideoProfileLevelQuerySupported:  {
      OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE *>(params);
      err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(prof_lvl->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
        if (port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC) {
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
          OMX_U32 idx = prof_lvl->nIndex;
#else
          OMX_U32 idx = prof_lvl->nProfileIndex;
#endif
          if (idx >= NELM(kAvcProfileLevel)){
            err = OMX_ErrorNoMore;
          } else {
            prof_lvl->eProfile = kAvcProfileLevel[idx].eProfile;
            prof_lvl->eLevel= kAvcProfileLevel[idx].eLevel;
          }
        } else {
          err = OMX_ErrorNoMore;
        }
      }
      break;
    }
    case OMX_IndexParamVideoProfileLevelCurrent:{
      OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE *>(params);
      err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(prof_lvl->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
        if (port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC) {
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
          OMX_U32 idx = prof_lvl->nIndex;
#else
          OMX_U32 idx = prof_lvl->nProfileIndex;
#endif
          prof_lvl->eProfile = kAvcProfileLevel[idx].eProfile;
          prof_lvl->eLevel= kAvcProfileLevel[idx].eLevel;
        }
      }
      break;
    }
    case OMX_IndexParamVideoAvc: {
      OMX_VIDEO_PARAM_AVCTYPE* codec_param =
          reinterpret_cast<OMX_VIDEO_PARAM_AVCTYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo &&
          port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC)
        *codec_param = ((OmxAvcPort*)port)->getCodecParam();
      break;
    }
    case OMX_IndexParamVideoH263:
      break;
    case OMX_IndexParamVideoMpeg4:
      break;
#ifdef _ANDROID_
    case OMX_IndexAndroidGetNativeBufferUsage: {
      android::GetAndroidNativeBufferUsageParams* getUsage_nativebuffer =
          reinterpret_cast<android::GetAndroidNativeBufferUsageParams*>(params);
      getUsage_nativebuffer->nUsage = 0;
      OMX_LOGD("GetNativeBufferUsage %lu", getUsage_nativebuffer->nUsage);
      break;
    }
#endif
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoDecoder::setParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamVideoPortFormat: {
      OMX_VIDEO_PARAM_PORTFORMATTYPE* video_param =
          reinterpret_cast<OMX_VIDEO_PARAM_PORTFORMATTYPE*>(params);
      err = CheckOmxHeader(video_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(video_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
        if (isVideoParamSupport(video_param)) {
          port->setVideoParam(video_param);
          port->updateDomainDefinition();
        } else {
          // TODO: find a proper error NO.
          err = OMX_ErrorNotImplemented;
        }
      }
      break;
    }
    case OMX_IndexParamVideoProfileLevelCurrent:  {
      OMX_VIDEO_PARAM_PROFILELEVELTYPE* prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE*>(params);
      err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(prof_lvl->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
        if (prof_lvl->eCodecType == OMX_VIDEO_CodingAVC) {
          if (prof_lvl->nIndex >= NELM(kAvcProfileLevel)){
            err = OMX_ErrorBadParameter;
            break;
          }
          mProfile = prof_lvl->eProfile;
          mLevel = prof_lvl->eLevel;
        } else {
          err = OMX_ErrorNotImplemented;
        }
#else
        if (isProfileLevelSupport(prof_lvl)){
          mProfile = prof_lvl->eProfile;
          mLevel = prof_lvl->eLevel;
        } else {
          err = OMX_ErrorNotImplemented;
        }
#endif
      }
      break;
    }
#ifdef _ANDROID_
    case OMX_IndexAndroidEnableNativeBuffers: {
      OMX_LOGD("EnableNativeBuffers");
      android::EnableAndroidNativeBuffersParams *enable_nativebuffer =
          reinterpret_cast<android::EnableAndroidNativeBuffersParams *>(params);
      err = CheckOmxHeader(enable_nativebuffer);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("EnableNativeBuffers error %d", err);
        break;
      }
      if (enable_nativebuffer->nPortIndex < 1) {
        err = OMX_ErrorBadPortIndex;
        OMX_LOGE("EnableNativeBuffers error %d", err);
        break;
      }
      OmxPortImpl *port = getPort(enable_nativebuffer->nPortIndex);
      if (!port) {
        err = OMX_ErrorUndefined;
        OMX_LOGE("EnableNativeBuffers error %d", err);
        break;
      }
      mUseNativeBuffers = enable_nativebuffer->enable;
      OMX_U32 size;
      OMX_U32 stride, height;
      OMX_COLOR_FORMATTYPE eFormat;
      OMX_VIDEO_PORTDEFINITIONTYPE &video_def = port->getVideoDefinition();
      stride = video_def.nFrameWidth;
      height = video_def.nFrameHeight;
      eFormat = static_cast<OMX_COLOR_FORMATTYPE>(HAL_PIXEL_FORMAT_YV12); // YCrCb 4:2:0 Planar
      stride = getAlignedStride(eFormat, stride);
      height = getAlignedStride(eFormat, height);
      video_def.nFrameWidth = stride;
      video_def.nFrameHeight = height;
      size = getNativeBufferSize(eFormat, stride, height);
      port->setBufferSize(size);
      port->updateDomainParameter();
      video_def.eColorFormat = eFormat;
      if (stride <= 176 && height <= 144) {
        // Use 64k input buffer for resolution below QCIF.
        OmxPortImpl *in_port = getPort(kVideoPortStartNumber);
        if (in_port) {
          in_port->setBufferSize(65536);
        }
      }
      break;
    }
    case OMX_IndexAndroidUseNativeBuffer: {
      OMX_LOGD("UseNativeBuffer");
      android::UseAndroidNativeBufferParams*  use_nativebuffer =
          reinterpret_cast<android::UseAndroidNativeBufferParams*>(params);
      err = CheckOmxHeader(use_nativebuffer);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("UseNativeBuffer error %d", err);
        break;
      }
      if (!mUseNativeBuffers) {
        err = OMX_ErrorUndefined;
        OMX_LOGE("UseNativeBuffer error %d", err);
        break;
      }
      if (use_nativebuffer->nPortIndex < 1) {
        err = OMX_ErrorBadPortIndex;
        OMX_LOGE("UseNativeBuffer error %d", err);
        break;
      }
      OmxPortImpl* port = getPort(use_nativebuffer->nPortIndex);
      if (!port) {
        err = OMX_ErrorUndefined;
        OMX_LOGE("UseNativeBuffer error %d", err);
        break;
      }
      OMX_BUFFERHEADERTYPE **bufHdr = use_nativebuffer->bufferHeader;
      GraphicBuffer *gfx = static_cast<GraphicBuffer *>(
          use_nativebuffer->nativeBuffer.get());
      uint32_t usage = GraphicBuffer::USAGE_SW_READ_OFTEN |
          GraphicBuffer::USAGE_SW_WRITE_RARELY;
      void *buf = NULL;
      gfx->lock(usage, &buf);
      OMX_LOGD("gfx format = 0x%x, Stride = %d, Height = %d, native buffer size is %d",
          gfx->format, gfx->getStride(), gfx->getHeight(),
          getNativeBufferSize(gfx->format, gfx->getStride(), gfx->getHeight()));
      err = useBuffer(
          bufHdr,
          use_nativebuffer->nPortIndex,
          use_nativebuffer->pAppPrivate,
          getNativeBufferSize(gfx->format, gfx->getStride(), gfx->getHeight()),
          static_cast<OMX_U8 *>(buf));
      (*bufHdr)->pPlatformPrivate = gfx;
      gfx->unlock();
      break;
    }
#endif
    case OMX_IndexParamVideoAvc: {
      OMX_VIDEO_PARAM_AVCTYPE* codec_param =
          reinterpret_cast<OMX_VIDEO_PARAM_AVCTYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo &&
          port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC)
        static_cast<OmxAvcPort *>(port)->setCodecParam(codec_param);
      break;
    }
    case OMX_IndexParamVideoH263:
      break;
    case OMX_IndexParamVideoMpeg4:
      break;
    default:
      return OmxComponentImpl::setParameter(index, params);

  }
  return err;
}

OMX_BOOL OmxVideoDecoder::isVideoParamSupport(
    OMX_VIDEO_PARAM_PORTFORMATTYPE* video_param) {
  // TODO: implement this
  OMX_BOOL isSupport = OMX_FALSE;
  switch (mInputPort->getVideoDefinition().eCompressionFormat) {
    case OMX_VIDEO_CodingAVC:
    case OMX_VIDEO_CodingMPEG4:
    case OMX_VIDEO_CodingMPEG2:
    case OMX_VIDEO_CodingH263:
#ifdef _ANDROID_
    //case OMX_VIDEO_CodingVPX:
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_VIDEO_CodingVP8:
#endif
      isSupport = OMX_TRUE;
      break;
    default:
      isSupport = OMX_FALSE;
  }
  return isSupport;
}
OMX_BOOL OmxVideoDecoder::isProfileLevelSupport(
    OMX_VIDEO_PARAM_PROFILELEVELTYPE* prof_lvl) {
  // TODO: implement this
  OMX_BOOL isSupport = OMX_FALSE;
  switch(mInputPort->getVideoDefinition().eCompressionFormat) {
    case OMX_VIDEO_CodingAVC:
    case OMX_VIDEO_CodingMPEG4:
    case OMX_VIDEO_CodingMPEG2:
    case OMX_VIDEO_CodingH263:
#ifdef _ANDROID_
    //case OMX_VIDEO_CodingVPX:
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_VIDEO_CodingVP8:
#endif
      isSupport = OMX_TRUE;
      break;
    default:
      isSupport = OMX_FALSE;
  }
  return isSupport;
}

OMX_ERRORTYPE OmxVideoDecoder::processOutputBuffer() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *out_buf = NULL;

  if (mHasDecodedFrame && !mOutputPort->isEmpty()) {
    // Get output buffer
    out_buf = mOutputPort->popBuffer();
    if (!out_buf) {
      OMX_LOGD("failed to get out_buf");
      return OMX_ErrorNotReady;
    }
    // Read the decoded frame
    if (mHasDecodedFrame == 1) {
      if (mUseNativeBuffers) {
        GraphicBuffer *gfx = static_cast<GraphicBuffer*>(out_buf->pPlatformPrivate);
        uint32_t usage = GraphicBuffer::USAGE_HW_RENDER;
        void *buf = NULL;
        gfx->lock(usage, &buf);
        err = mFFmpegDecoder->fetchDecodedFrame((OMX_U8*)buf,
            &(out_buf->nFilledLen), &(out_buf->nTimeStamp));
        gfx->unlock();
      } else {
        // TODO: handle the non native buffer cases
        err = mFFmpegDecoder->fetchDecodedFrame(out_buf->pBuffer + out_buf->nOffset,
            &(out_buf->nFilledLen), &(out_buf->nTimeStamp));
      }
    } else if (mHasDecodedFrame == 2) {
      OMX_LOGD("The EOS input buffer is empty, just return an empty output buffer");
    }

    if ((OMX_TRUE == mEOS) || (out_buf && out_buf->nFilledLen !=0)) {
      if (out_buf) {
        if (mEOS) {
          OMX_LOGD("Video EOS reached, post event EOS!");
          out_buf->nFlags |= OMX_BUFFERFLAG_EOS;
        }
        //OMX_LOGD("Output buffer %p, pts %lld(%fs) fill done",
        //         out_buf, out_buf->nTimeStamp, out_buf->nTimeStamp / (float)1000000);
        returnBuffer(mOutputPort, out_buf);
      }
    }

    mHasDecodedFrame = 0;
  }

  return err;
}

OMX_ERRORTYPE OmxVideoDecoder::processInputBuffer() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *in_buf = NULL;

  if (!mInputPort->isEmpty()) {
    in_buf = mInputPort->popBuffer();

    // Process codec config data
    if (in_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
      if (mCodecSpecficDataSent) {
        vector<CodecSpeficDataType>::iterator it;
        while (!mCodecSpeficData.empty()) {
          it = mCodecSpeficData.begin();
          if (it->data != NULL) {
            free(it->data);
          }
          mCodecSpeficData.erase(it);
        }
      }
      CodecSpeficDataType extra_data;
      extra_data.data = malloc(in_buf->nFilledLen);
      extra_data.size = in_buf->nFilledLen;
      char *pData = reinterpret_cast<char *>(in_buf->pBuffer) + in_buf->nOffset;
      memcpy(extra_data.data, in_buf->pBuffer + in_buf->nOffset,
          in_buf->nFilledLen);
      mCodecSpeficData.push_back(extra_data);
      in_buf->nFilledLen = 0;
      in_buf->nOffset = 0;
      returnBuffer(mInputPort, in_buf);
      return err;
    }

    if (in_buf->nFlags & OMX_BUFFERFLAG_EOS) {
      OMX_LOGD("meet EOS in in_buf, waiting for stop");
      mEOS = OMX_TRUE;
    }

    // init decoder
    if (mInited == OMX_FALSE) {
      OMX_U32 codec_data_size = 0;
      vector<CodecSpeficDataType>::iterator it;
      for (it = mCodecSpeficData.begin();
           it != mCodecSpeficData.end(); it++) {
        codec_data_size += it->size;
      }
      OMX_U8 *data = (OMX_U8*)malloc(codec_data_size);
      OMX_U32 offset = 0;
      for (it = mCodecSpeficData.begin();
           it != mCodecSpeficData.end(); it++) {
        OMX_LOGD("Merge extra data %p, size %d", it->data, it->size);
        memcpy(data + offset, it->data, it->size);
        offset += it->size;
      }
      mFFmpegDecoder->init(getCodecType(),
          mOutputPort->getVideoDefinition().eColorFormat, data, codec_data_size);
      if (err != OMX_ErrorNone) {
        OMX_LOGE("Failed to init ffmpeg decoder!");
        postEvent(OMX_EventError, 0, 0);
        return err;
      }
      mInited = OMX_TRUE;
    }

    // Decode the input buffer
    OMX_U32 consume_size = 0, got_decoded_frame = 0;
    if (in_buf->nFilledLen > 0) {
      err = mFFmpegDecoder->decode(in_buf->pBuffer + in_buf->nOffset, in_buf->nFilledLen,
          in_buf->nTimeStamp, &consume_size, &mHasDecodedFrame);
      if (in_buf->nFilledLen >= consume_size) {
        in_buf->nFilledLen -= consume_size;
        in_buf->nOffset += consume_size;
      }
    } else {
      OMX_LOGD("in_buf->nFilledLen = 0, is an empty EOS input buffer");
      mHasDecodedFrame = 2;
    }
    if (in_buf->nFilledLen == 0) {
      //OMX_LOGD("Input buffer %p empty done", in_buf);
      in_buf->nOffset = 0;
      returnBuffer(mInputPort, in_buf);
    }
  }

  return err;
}


OMX_ERRORTYPE OmxVideoDecoder::processBuffer() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *in_buf = NULL, *out_buf = NULL;

  if (mEOS && !mHasDecodedFrame) {
    OMX_LOGD("EOS reached, waiting for stop");
    return OMX_ErrorNoMore;
  }

  if (mHasDecodedFrame && mOutputPort->isEmpty()) {
    // If one frame is decoded, but no output buffer, should hold decoding.
    return OMX_ErrorOverflow;
  }

  if (mHasDecodedFrame) {
    processOutputBuffer();
  }

  if (!mInputPort->isEmpty()) {
    processInputBuffer();
  }

  if (mHasDecodedFrame) {
    processOutputBuffer();
  }

  return err;
}

OMX_ERRORTYPE OmxVideoDecoder::flush() {
  mHasDecodedFrame = 0;
  mEOS = OMX_FALSE;
  return mFFmpegDecoder->flush();
}

OMX_ERRORTYPE OmxVideoDecoder::componentDeInit(
    OMX_HANDLETYPE hComponent) {
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxVideoDecoder *vdec = static_cast<OmxVideoDecoder *>(comp);
  delete vdec;
  return OMX_ErrorNone;
}

}

