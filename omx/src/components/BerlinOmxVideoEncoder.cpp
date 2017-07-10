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

#define LOG_TAG "BerlinOmxVideoEncoder"
#include <BerlinOmxVideoEncoder.h>
#include <BerlinOmxAmpPort.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _ANDROID_
#include <HardwareAPI.h>
#include <MetadataBufferType.h>
#include <libyuv/convert.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>
#endif

#define VENC_TRACE OMX_LOGD

namespace berlin {

static void dump_bytes(OMX_U8 *bits, size_t size) {
  static int first = 1;
  const char *mode = "a";
  if (first) {
    first = 0;
    mode = "w";
  }
  FILE *file = fopen("/data/omx_venc.dat", mode);
  if (file == NULL) {
    OMX_LOGE("Open file failed");
    assert(file != NULL);
  } else if (size > 0) {
    fwrite(bits, 1, size, file);
    fclose(file);
  }
}

enum
{
  VENC_YUV420_TILE = 0,
  VENC_YUV422_UYVY,
  VENC_YUV422_YUYV,
  VENC_YUV420_PLANE,
  VENC_YUV422_PLANE,
  VENC_YUV420_SP,
  VENC_YUV420_SPA,
  VENC_YUV420_YV12,
  VENC_INPUT_UNSUPPORTED
};

struct ProfileLevelType{
  OMX_U32 eProfile;
  OMX_U32 eLevel;
};

struct VideoFormatType{
  OMX_VIDEO_CODINGTYPE eCompressionFormat;
  OMX_COLOR_FORMATTYPE eColorFormat;
  OMX_U32 xFramerate;
};

const char kFrameI = 0x5;

static const VideoFormatType kCodecSupport[] = {
  {OMX_VIDEO_CodingAVC, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingMPEG4, OMX_COLOR_FormatUnused, 15 <<16},
  // {OMX_VIDEO_CodingH263, OMX_COLOR_FormatUnused, 60 <<16},
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  {OMX_VIDEO_CodingVP8, OMX_COLOR_FormatUnused, 15 <<16},
#endif
#ifdef _ANDROID_
  {static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVPX),
       OMX_COLOR_FormatUnused, 15 <<16},
#endif
};

static const VideoFormatType kColorSupport[] = {
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420Planar, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420SemiPlanar, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYCbYCr, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatCrYCbY, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYVU420Planar, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV422SemiPlanar, 15 <<16},
  {OMX_VIDEO_CodingUnused, static_cast<OMX_COLOR_FORMATTYPE>(
      OMX_COLOR_FormatAndroidOpaque), 15 <<16}
};

static const ProfileLevelType kAvcEncProfileLevel[] = {
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1},
  {OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel22},
  {OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51},
  {OMX_VIDEO_AVCProfileExtended, OMX_VIDEO_AVCLevel51}
};

static const ProfileLevelType kMpeg4EncProfileLevel[] = {
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0},
  {OMX_VIDEO_MPEG4ProfileMain, OMX_VIDEO_MPEG4Level5},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level5}
};

static const ProfileLevelType kH263EncProfileLevel[] = {
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10}
};

static const ProfileLevelType kVp8EncProfileLevel[] = {
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version0},
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version1},
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version2},
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version3}
};

OMX_S32 getAmpColorFormat(OMX_COLOR_FORMATTYPE omx_color) {
  OMX_LOGD("Input color format 0x%x", omx_color);
  switch(omx_color) {
    // TODO: Check how to map VENC_YUV420_TILE & VENC_YUV420_SP to OMX
    //       color format.
    case OMX_COLOR_FormatCrYCbY: return VENC_YUV422_UYVY;
    case OMX_COLOR_FormatYCbYCr: return VENC_YUV422_YUYV;
    case OMX_COLOR_FormatYUV422SemiPlanar: return VENC_YUV422_UYVY;
    case OMX_COLOR_FormatYUV422Planar: return VENC_YUV422_PLANE;
    case OMX_COLOR_FormatYUV420Planar: return VENC_YUV420_PLANE;
    case OMX_COLOR_FormatYUV420SemiPlanar: return VENC_YUV420_SPA;
    case OMX_COLOR_FormatYVU420Planar: return VENC_YUV420_YV12;
#ifdef _ANDROID_
    case OMX_COLOR_FormatAndroidOpaque: return VENC_YUV420_PLANE;
#endif
    default:
      OMX_LOGW("Unsupport color format 0x%x", omx_color);
      return VENC_INPUT_UNSUPPORTED;
  }
}

OMX_U32 getFrameSize(OMX_U32 width, OMX_U32 height, OMX_COLOR_FORMATTYPE color) {
  switch(color) {
    case OMX_COLOR_FormatYUV422Planar:
    case OMX_COLOR_FormatYUV422SemiPlanar:
    case OMX_COLOR_FormatCrYCbY:
    case OMX_COLOR_FormatYCbYCr:
    case OMX_COLOR_FormatYCrYCb:
      return width * height * 2;
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
#ifdef _ANDROID_
    case OMX_COLOR_FormatAndroidOpaque:
#endif
      return width * height * 3 / 2;
    default:
      OMX_LOGW("Unsupport color format 0x%x", color);
      return 0;
  }
}

OmxVideoEncoderPort::OmxVideoEncoderPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  mDefinition.nBufferCountMin = 8;
  mDefinition.nBufferCountActual = 8;
  mDefinition.format.video.pNativeRender = NULL;
  mDefinition.format.video.nFrameWidth = 176;
  mDefinition.format.video.nFrameHeight = 144;
  mDefinition.format.video.nStride =  176;
  mDefinition.format.video.nSliceHeight =  144;
  mDefinition.format.video.nBitrate = 64000;
  mDefinition.format.video.xFramerate = 15 << 16;
  mDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
  mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  mDefinition.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
  mDefinition.format.video.pNativeWindow = NULL;
  InitOmxHeader(&mVideoParam);
  updateDomainParameter();
  mReturnedBuffer = NULL;
  mPushedBdNum = 0;
  mReturnedBdNum = 0;
  mAmpHandle = NULL;
  mEos = OMX_FALSE;
}

OmxVideoEncoderPort::~OmxVideoEncoderPort() {
}

void OmxVideoEncoderPort::updateDomainParameter() {
  mVideoParam.eColorFormat = getVideoDefinition().eColorFormat;
  mVideoParam.eCompressionFormat = getVideoDefinition().eCompressionFormat;
  mVideoParam.xFramerate = getVideoDefinition().xFramerate;
  mVideoParam.nPortIndex = getPortIndex();
}

void OmxVideoEncoderPort::updateDomainDefinition() {
  mDefinition.format.video.eColorFormat = mVideoParam.eColorFormat;
  mDefinition.format.video.eCompressionFormat = mVideoParam.eCompressionFormat;
  mDefinition.format.video.xFramerate = mVideoParam.xFramerate;
}

OmxVideoEncoderInputPort::OmxVideoEncoderInputPort(OMX_U32 index) :
    OmxVideoEncoderPort(index, OMX_DirInput) {
  mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  mDefinition.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
  mFrameSize = 0;
}

OmxVideoEncoderInputPort::~OmxVideoEncoderInputPort() {
}

void OmxVideoEncoderInputPort::setDefinition(OMX_PARAM_PORTDEFINITIONTYPE *definition) {
  mDefinition = *definition;
  OMX_VIDEO_PORTDEFINITIONTYPE &in_def = getVideoDefinition();
  mFrameSize = getFrameSize(in_def.nFrameWidth, in_def.nFrameHeight, in_def.eColorFormat);
  if (mDefinition.nBufferSize <= mFrameSize) {
    mDefinition.nBufferSize = mFrameSize + 20;
  }
  mWidth = in_def.nFrameWidth;
  mHeight = in_def.nFrameHeight;
}

OMX_ERRORTYPE OmxVideoEncoderInputPort::getVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param) {
  if (param->nIndex >= NELM(kColorSupport)) {
    return OMX_ErrorNoMore;
  }
  const VideoFormatType *v_fmt = &kColorSupport[param->nIndex];
  param->eCompressionFormat = v_fmt->eCompressionFormat;
  param->eColorFormat = v_fmt->eColorFormat;
  param->xFramerate = v_fmt->xFramerate;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoEncoderInputPort::setVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param) {
  int i;
  for (i = 0; i < NELM(kColorSupport); i++) {
    if(param->eColorFormat == kColorSupport[i].eColorFormat) {
      break;
    }
  }
  if (i >= NELM(kColorSupport)) {
    OMX_LOGW("Unsupport color 0x%x", param->eColorFormat);
    return OMX_ErrorNoMore;
  }
  mVideoParam.eColorFormat = param->eColorFormat;
  mVideoParam.eCompressionFormat = param->eCompressionFormat;
  mVideoParam.xFramerate = param->xFramerate;
  updateDomainDefinition();
  return OMX_ErrorNone;
}

void OmxVideoEncoderInputPort::pushBuffer(OMX_BUFFERHEADERTYPE *buffer) {
  kdThreadMutexLock(mBufferMutex);
  setBufferOwner(buffer, OWN_BY_OMX);
  AmpBuffer *amp_buffer =  static_cast<AmpBuffer *>(buffer->pPlatformPrivate);
  // TODO: Workaround for camera buffer size
  if (mFrameSize > 0) {
    buffer->nFilledLen = mFrameSize;
#ifdef _ANDROID_
    if (mStoreMetaDataInBuffers && buffer->nFilledLen >= 8
        && (buffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_FALSE) {
      buffer_handle_t srcBuffer; // for MetaDataMode only
      OMX_U8 *dst = buffer->pBuffer + buffer->nOffset;
      OMX_U8 *src = extractGrallocData(dst, &srcBuffer);
      kdMemcpy(dst + mFrameSize, dst, 8);
      OMX_U32 y_size = mWidth * mHeight;
      libyuv::ABGRToI420(src, 4 * mWidth,
          dst, mWidth,
          dst + y_size, mWidth >> 1,
          dst + y_size * 5 / 4, mWidth >> 1,
          mWidth, mHeight);
    }
#endif
  }
  amp_buffer->updateFrameInfo(buffer);
  amp_buffer->updateMemInfo(buffer);
  if((buffer->nFlags & OMX_BUFFERFLAG_EOS) == 0) {
    mEncoder->mTsRecover.insertTimeStamp(buffer->nTimeStamp);
  }
  mPushedBdNum++;
  HRESULT err = SUCCESS;
  AMP_BD_ST *bd = amp_buffer->getBD();
  OMX_LOGD("<In> %d/%d, push bd %p, pts %lld, flag 0x%x",
      mReturnedBdNum, mPushedBdNum, bd, buffer->nTimeStamp, buffer->nFlags);
  AMP_RPC(err, AMP_VENC_PushBD, mAmpHandle, AMP_PORT_INPUT, 0, bd);
  setBufferOwner(buffer, OWN_BY_LOWER);
  kdThreadMutexUnlock(mBufferMutex);
}

#ifdef _ANDROID_
OMX_U8 * OmxVideoEncoderInputPort::extractGrallocData(void *data, buffer_handle_t *buffer) {
  OMX_U32 type = *(OMX_U32*)data;
  if (type != kMetadataBufferTypeGrallocSource) {
    OMX_LOGE("Wrong buffer type");
    return NULL;
  }
  buffer_handle_t imgBuffer = *(buffer_handle_t*)((uint8_t*)data + 4);

  const Rect rect(mWidth, mHeight);
  uint8_t *img;
  status_t res = GraphicBufferMapper::get().lock(imgBuffer,
      GRALLOC_USAGE_HW_VIDEO_ENCODER,
      rect, (void**)&img);
  if (res != OK) {
    OMX_LOGE("%s: Unable to lock image buffer %p for access", __FUNCTION__,
        imgBuffer);
    return NULL;
  }
  OMX_LOGV("lock buffer_handle %p", imgBuffer);
  *buffer = imgBuffer;
  return img;
}

void OmxVideoEncoderInputPort::releaseGrallocData(buffer_handle_t buffer) {
  OMX_LOGV("unlock buffer_handle %p", buffer);
  GraphicBufferMapper::get().unlock(buffer);
}
#endif

OmxVideoEncoderOutputPort::OmxVideoEncoderOutputPort(OMX_U32 index) :
  OmxVideoEncoderPort(index, OMX_DirOutput){
  mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAutoDetect;
  mDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
  mIsExtraDataGot = OMX_FALSE;
}

OmxVideoEncoderOutputPort::~OmxVideoEncoderOutputPort() {
}

OMX_ERRORTYPE OmxVideoEncoderOutputPort::getVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param) {
  if (param->nIndex >= NELM(kCodecSupport)) {
    return OMX_ErrorNoMore;
  }
  const VideoFormatType *v_fmt = &kCodecSupport[param->nIndex];
  param->eCompressionFormat = v_fmt->eCompressionFormat;
  param->eColorFormat = v_fmt->eColorFormat;
  param->xFramerate = v_fmt->xFramerate;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoEncoderOutputPort::setVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *param) {
  int i;
  for (i = 0; i < NELM(kCodecSupport); i++) {
    if(param->eCompressionFormat == kCodecSupport[i].eCompressionFormat) {
      break;
    }
  }
  if (i >= NELM(kCodecSupport)) {
    OMX_LOGW("Unsupport codec 0x%x", param->eCompressionFormat);
    return OMX_ErrorNoMore;
  }
  mVideoParam.eColorFormat = param->eColorFormat;
  mVideoParam.eCompressionFormat = param->eCompressionFormat;
  mVideoParam.xFramerate = param->xFramerate;
  updateDomainDefinition();
  return OMX_ErrorNone;
}

void OmxVideoEncoderOutputPort::pushBuffer(OMX_BUFFERHEADERTYPE *buffer) {
  kdThreadMutexLock(mBufferMutex);
  setBufferOwner(buffer, OWN_BY_OMX);
  if (mEos) {
    mBufferQueue.push(buffer);
    kdThreadSemPost(mBufferSem);
    kdThreadMutexUnlock(mBufferMutex);
    return;
  }
  AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(buffer->pPlatformPrivate);
  // TODO: Workaround for amp bd mem_info size
  buffer->nFilledLen = buffer->nAllocLen;
  amp_buffer->updateMemInfo(buffer);
  mPushedBdNum++;
  HRESULT err = SUCCESS;
  AMP_BD_ST *bd = amp_buffer->getBD();
  OMX_LOGD("<Out> %d/%d, push bd %p", mReturnedBdNum, mPushedBdNum, bd);
  AMP_RPC(err, AMP_VENC_PushBD, mAmpHandle, AMP_PORT_OUTPUT, 0, bd);
  setBufferOwner(buffer, OWN_BY_LOWER);
  kdThreadMutexUnlock(mBufferMutex);
}

OmxVideoEncoder::OmxVideoEncoder() {
}

OmxVideoEncoder::OmxVideoEncoder(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
  mEOS = OMX_FALSE;
  mOutputEOS = OMX_FALSE;
  mProfileLevelNum = 0;
  mFramerateQ16 = 0;
}

OmxVideoEncoder::~OmxVideoEncoder() {
  destroyAmpVenc();
}

OMX_ERRORTYPE OmxVideoEncoder::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char *role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_VIDEO_ENCODER_AVC,
               strlen(OMX_ROLE_VIDEO_ENCODER_AVC))) {
    addRole(OMX_ROLE_VIDEO_ENCODER_AVC);
  } else if (!strncmp(role_name, OMX_ROLE_VIDEO_ENCODER_VP8,
                      strlen(OMX_ROLE_VIDEO_ENCODER_VP8))) {
    addRole(OMX_ROLE_VIDEO_ENCODER_VP8);
  } else if (!strncmp(role_name, OMX_ROLE_VIDEO_ENCODER_MPEG4,
                      strlen(OMX_ROLE_VIDEO_ENCODER_MPEG4))) {
    addRole(OMX_ROLE_VIDEO_ENCODER_MPEG4);
  } else if (!strncmp(role_name, OMX_ROLE_VIDEO_ENCODER_H263,
                      strlen(OMX_ROLE_VIDEO_ENCODER_H263))) {
    addRole(OMX_ROLE_VIDEO_ENCODER_H263);
  } else if (!strncmp(role_name, "video_encoder", 13)) {
    addRole(OMX_ROLE_VIDEO_ENCODER_AVC);
    addRole(OMX_ROLE_VIDEO_ENCODER_VP8);
    addRole(OMX_ROLE_VIDEO_ENCODER_H263);
    addRole(OMX_ROLE_VIDEO_ENCODER_MPEG4);
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoEncoder::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mInputPort = new OmxVideoEncoderInputPort(kVideoPortStartNumber);
  mInputPort->mEncoder = this;
  addPort(mInputPort);
  mOutputPort = new OmxVideoEncoderOutputPort(kVideoPortStartNumber + 1);
  mOutputPort->mEncoder = this;
  addPort(mOutputPort);
  OMX_VIDEO_PORTDEFINITIONTYPE &out_def = mOutputPort->getVideoDefinition();
  OMX_LOGD("role %s", mActiveRole);
  if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_ENCODER_AVC, 17)) {
    out_def.eCompressionFormat = OMX_VIDEO_CodingAVC;
    InitOmxHeader(mCodecParam.avc);
    mCodecParam.avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
    mCodecParam.avc.eLevel = OMX_VIDEO_AVCLevel1;
    mProfileLevelPtr = kAvcEncProfileLevel;
    mProfileLevelNum = NELM(kAvcEncProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_ENCODER_MPEG4, 19)) {
    out_def.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
    InitOmxHeader(mCodecParam.mpeg4);
    mCodecParam.mpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    mCodecParam.mpeg4.eLevel = OMX_VIDEO_MPEG4Level0;
    mProfileLevelPtr = kMpeg4EncProfileLevel;
    mProfileLevelNum = NELM(kMpeg4EncProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_ENCODER_H263, 18)) {
    out_def.eCompressionFormat = OMX_VIDEO_CodingH263;
    InitOmxHeader(mCodecParam.h263);
    mCodecParam.h263.eProfile = OMX_VIDEO_H263ProfileBaseline;
    mCodecParam.h263.eLevel = OMX_VIDEO_H263Level10;
    mProfileLevelPtr = kH263EncProfileLevel;
    mProfileLevelNum = NELM(kH263EncProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_ENCODER_VP8, 17)) {
    out_def.eCompressionFormat = OMX_VIDEO_CodingVP8;
  }
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::getParameter(OMX_INDEXTYPE index,
    OMX_PTR params) {
  OMX_LOGV("%s, index 0x%x, params %p", __func__, index, params);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamVideoPortFormat: {
      OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param =
          reinterpret_cast<OMX_VIDEO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(video_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(video_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      err = port->getVideoParam(video_param);
      break;
    }
    case OMX_IndexParamVideoProfileLevelQuerySupported: {
      OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE *>(params);
      // TODO: keep compatible with 1.1 spec
      /*
      err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }
      */
      OmxPortImpl *port = getPort(prof_lvl->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
      OMX_U32 idx = prof_lvl->nIndex;
#else
      OMX_U32 idx = prof_lvl->nProfileIndex;
#endif
      if (idx >= mProfileLevelNum) {
        err = OMX_ErrorNoMore;
      } else {
        prof_lvl->eProfile = mProfileLevelPtr[idx].eProfile;
        prof_lvl->eLevel= mProfileLevelPtr[idx].eLevel;
      }
      OMX_LOGD("Proflie (%d), level (%d), err 0x%x",
          prof_lvl->eProfile, prof_lvl->eLevel, err);
      break;
    }
    case OMX_IndexParamVideoProfileLevelCurrent:{
      OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE *>(params);
      err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(prof_lvl->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
      OMX_U32 idx = prof_lvl->nIndex;
#else
      OMX_U32 idx = prof_lvl->nProfileIndex;
#endif
      if (idx >= mProfileLevelNum){
        err = OMX_ErrorNoMore;
      } else {
        prof_lvl->eProfile = mProfileLevelPtr[idx].eProfile;
        prof_lvl->eLevel= mProfileLevelPtr[idx].eLevel;
      }
      break;
    }
    case OMX_IndexParamVideoBitrate:
    case OMX_IndexParamVideoErrorCorrection:
      break;
    case OMX_IndexParamVideoAvc: {
      OMX_VIDEO_PARAM_AVCTYPE *avc_param =
          reinterpret_cast<OMX_VIDEO_PARAM_AVCTYPE *>(params);
      err = CheckOmxHeader(avc_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(avc_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      *avc_param = mCodecParam.avc;
      break;
    }
    case OMX_IndexParamVideoH263: {
      OMX_VIDEO_PARAM_H263TYPE *h263_param =
          reinterpret_cast<OMX_VIDEO_PARAM_H263TYPE *>(params);
      err = CheckOmxHeader(h263_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(h263_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      *h263_param = mCodecParam.h263;
      break;
    }
    case OMX_IndexParamVideoMpeg4: {
            OMX_VIDEO_PARAM_MPEG4TYPE *mpeg4_param =
          reinterpret_cast<OMX_VIDEO_PARAM_MPEG4TYPE *>(params);
      err = CheckOmxHeader(mpeg4_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(mpeg4_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      *mpeg4_param = mCodecParam.mpeg4;
      break;
    }
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::setParameter(OMX_INDEXTYPE index,
    OMX_PTR params) {
  OMX_LOGV("%s, index 0x%x, params %p", __func__, index, params);
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamVideoPortFormat: {
      OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param =
          reinterpret_cast<OMX_VIDEO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(video_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(video_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      err = port->setVideoParam(video_param);
      break;
    }
    case OMX_IndexParamVideoProfileLevelCurrent:
    case OMX_IndexParamVideoBitrate:
    case OMX_IndexParamVideoErrorCorrection:
      break;
    case OMX_IndexParamVideoAvc: {
      // TODO: check the avc encode setting
      break;
    }
    case OMX_IndexParamVideoH263:
    case OMX_IndexParamVideoMpeg4:
      break;
#ifdef _ANDROID_
    case OMX_IndexAndroidEnableNativeBuffers: {
      android::EnableAndroidNativeBuffersParams *enable_nativebuffer =
          reinterpret_cast<android::EnableAndroidNativeBuffersParams*>(params);
      err = CheckOmxHeader(enable_nativebuffer);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("EnableNativeBuffers error %d", err);
        break;
      }
      OMX_LOGD("EnableNativeBuffers port %d, enable %d",
          enable_nativebuffer->nPortIndex, enable_nativebuffer->enable);
      break;
    }
    case OMX_IndexAndroidStoreMetadataInBuffers: {
      StoreMetaDataInBuffersParams *storeParams =
          (StoreMetaDataInBuffersParams*)params;
      if (storeParams->nPortIndex != 0) {
        OMX_LOGE("StoreMetadataInBuffersParams port index is not 0");
        return OMX_ErrorBadPortIndex;
      }
      mInputPort->mStoreMetaDataInBuffers = storeParams->bStoreMetaData;
      OMX_LOGD("StoreMetaDataInBuffers set to: %s",
          mInputPort->mStoreMetaDataInBuffers ? " true" : "false");
      break;
    }
#endif
    default:
      return OmxComponentImpl::setParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::getConfig(OMX_INDEXTYPE index,
    OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGV("%s, index 0x%x, config %p", __func__, index, config);
  switch (index) {
    case OMX_IndexConfigVideoFramerate: {
      OMX_CONFIG_FRAMERATETYPE *frame_rate =
          reinterpret_cast<OMX_CONFIG_FRAMERATETYPE *>(config);
      err = CheckOmxHeader(frame_rate);
      if (OMX_ErrorNone != err) {
        break;
      }
      frame_rate->xEncodeFramerate = mFramerateQ16;
      break;
    }
    case OMX_IndexConfigVideoBitrate:
      break;
    default:
      return OmxComponentImpl::getConfig(index, config);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::setConfig(OMX_INDEXTYPE index,
    OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGV("%s, index 0x%x, config %p", __func__, index, config);
  switch (index) {
    case OMX_IndexConfigVideoFramerate: {
      OMX_CONFIG_FRAMERATETYPE *frame_rate =
          reinterpret_cast<OMX_CONFIG_FRAMERATETYPE *>(config);
      err = CheckOmxHeader(frame_rate);
      if (OMX_ErrorNone != err) {
        break;
      }
      mFramerateQ16 = frame_rate->xEncodeFramerate;
      break;
    }
    case OMX_IndexConfigVideoBitrate:
      break;
    default:
      return OmxComponentImpl::setConfig(index, config);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::createAmpVenc() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_FACTORY factory;
  err = AMP_GetFactory(&factory);
  CHECKAMPERR(err);
  AMP_RPC(err, AMP_FACTORY_CreateComponent, factory, AMP_COMPONENT_VENC, 1,
      &mAmpHandle);
  CHECKAMPERR(err);
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::destroyAmpVenc() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  if (mAmpHandle) {
    AMP_RPC(err, AMP_VENC_Destroy, mAmpHandle);
    CHECKAMPERR(err);
    AMP_FACTORY_Release(mAmpHandle);
    mAmpHandle = NULL;
  }
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

void OmxVideoEncoder::clearPort() {
  // Waiting for input/output buffers back
  OMX_U32 wait_count = 100;
  while ((mInputPort->mPushedBdNum > mInputPort->mReturnedBdNum
      || mOutputPort->mPushedBdNum > mOutputPort->mReturnedBdNum)
      && wait_count > 0) {
    usleep(5000);
    wait_count--;
  }
  if (!wait_count) {
    OMX_LOGE("There are %u inbuf and %u outbuf have not returned.",
        mInputPort->mPushedBdNum - mInputPort->mReturnedBdNum,
        mOutputPort->mPushedBdNum - mOutputPort->mReturnedBdNum);
  }
  if (mInputPort->mReturnedBuffer != NULL) {
    returnBuffer(mInputPort, mInputPort->mReturnedBuffer);
    mInputPort->mReturnedBuffer = NULL;
  }
  if (mOutputPort->mReturnedBuffer != NULL) {
    returnBuffer(mOutputPort, mOutputPort->mReturnedBuffer);
    mOutputPort->mReturnedBuffer = NULL;
  }
  mOutputPort->mIsExtraDataGot = OMX_FALSE;
  mInputPort->mPushedBdNum = 0;
  mInputPort->mReturnedBdNum = 0;
  mOutputPort->mPushedBdNum = 0;
  mOutputPort->mReturnedBdNum = 0;
  while(!mOutputPort->isEmpty()){
    OMX_BUFFERHEADERTYPE *tmp_buf = mOutputPort->popBuffer();
    returnBuffer(mOutputPort, tmp_buf);
  }
  mInputPort->mEos = OMX_FALSE;
  mOutputPort->mEos = OMX_FALSE;
}

HRESULT OmxVideoEncoder::bufferCb(
    AMP_COMPONENT component,
    AMP_PORT_IO port_Io,
    UINT32 port_idx,
    AMP_BD_ST *bd, void *context) {
  OMX_LOGV("%s, port_IO %d, bd 0x%x, bd.bdid 0x%x, bd.allocva 0x%x",
      __func__, port_Io, bd, bd->uiBDId, bd->uiAllocVA);
  HRESULT err = SUCCESS;
  OMX_BUFFERHEADERTYPE *bufHdr = NULL;
  OMX_BOOL isLastOutFrame = OMX_FALSE;
  OmxVideoEncoder *encoder = static_cast<OmxVideoEncoder *>(context);
  if (AMP_PORT_INPUT == port_Io && 0 == port_idx) {
    OmxVideoEncoderInputPort *port = encoder->mInputPort;
    bufHdr = port->getBufferHeader(bd);
    if (NULL != bufHdr) {
#ifdef _ANDROID_
      if (port->mStoreMetaDataInBuffers && (
          bufHdr->nFlags & OMX_BUFFERFLAG_EOS) == OMX_FALSE) {
        kdMemcpy(bufHdr->pBuffer + bufHdr->nOffset,
            bufHdr->pBuffer + bufHdr->nOffset + port->mFrameSize,
            8);
        port->releaseGrallocData(*(buffer_handle_t*)(
            bufHdr->pBuffer + bufHdr->nOffset + port->mFrameSize + 4));
      }
#endif
      if (port->mReturnedBuffer != NULL) {
        encoder->returnBuffer(port, port->mReturnedBuffer);
      }
      port->mReturnedBuffer = bufHdr;
      port->mReturnedBdNum++;
      OMX_LOGD("[In] %d/%d", port->mReturnedBdNum, port->mPushedBdNum);
    }
  } else if (AMP_PORT_OUTPUT == port_Io && 0 == port_idx) {
    OmxVideoEncoderOutputPort *port = encoder->mOutputPort;
    bufHdr = port->getBufferHeader(bd);
    if (NULL != bufHdr) {
      if (encoder->mMark.hMarkTargetComponent != NULL) {
        bufHdr->hMarkTargetComponent = encoder->mMark.hMarkTargetComponent;
        bufHdr->pMarkData = encoder->mMark.pMarkData;
        encoder->mMark.hMarkTargetComponent = NULL;
        encoder->mMark.pMarkData = NULL;
      }
      AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
          bufHdr->pPlatformPrivate);
      AMP_BDTAG_STREAM_INFO *stream_info = NULL;
      err = amp_buffer->getStreamInfo(&stream_info);
      if (stream_info == NULL) {
        OMX_LOGE("amp_buffer get stream_info return 0x%x", err);
        return err;
      }
      OMX_LOGV("IsHeader %d, Frame type %d, pts 0x%x - %x",
          stream_info->isHeader,
          stream_info->uType,
          stream_info->uiPtsHigh,
          stream_info->uiPtsLow);
      OMX_TICKS pts_low, pts_high;
      pts_low = static_cast<OMX_TICKS>(stream_info->uiPtsLow);
      pts_high = static_cast<OMX_TICKS>(stream_info->uiPtsHigh);
      if (stream_info->uiPtsHigh & TIME_STAMP_VALID_MASK) {
        encoder->mTsRecover.findTimeStamp(pts_high, pts_low, &bufHdr->nTimeStamp);
      }
      AMP_BDTAG_MEMINFO *mem_info = NULL;
      err = amp_buffer->getMemInfo(&mem_info);
      if (mem_info == NULL) {
        OMX_LOGE("amp_buffer get getMemInfo return 0x%x", err);
        return err;
      }
      if (stream_info->isHeader) {
        if (mem_info->uSize > 0) {
          if(port->mIsExtraDataGot == OMX_FALSE) {
            bufHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
            port->mIsExtraDataGot = OMX_TRUE;
          } else {
            // TODO: Workaround for amp bd mem_info size
            OMX_LOGW("Return repeated stream header");
            mem_info->uSize = bufHdr->nAllocLen;
            AMP_RPC(err, AMP_VENC_PushBD, encoder->mAmpHandle, AMP_PORT_OUTPUT, 0, bd);
            return err;
          }
        }
      } else if (stream_info->uType == 0) {
         bufHdr->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
      }

      if (mem_info->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
        OMX_LOGD("[Out] Receive EOS");
        bufHdr->nFlags = OMX_BUFFERFLAG_EOS;
        port->mEos = OMX_TRUE;
      }
      if (port->mEos == OMX_FALSE && mem_info->uSize == 0) {
        OMX_LOGW("Set 1st zero filled output buffer EOS flag");
        bufHdr->nFlags = OMX_BUFFERFLAG_EOS;
        port->mEos = OMX_TRUE;
      }
      bufHdr->nFilledLen = mem_info->uSize;;
      // dump_bytes(bufHdr->pBuffer, bufHdr->nFilledLen);
      encoder->returnBuffer(port, bufHdr);
      port->mReturnedBdNum++;
      OMX_LOGD("[Out] %d/%d, size %d, pts %lld, flag %x",
          port->mReturnedBdNum, port->mPushedBdNum, bufHdr->nFilledLen,
          bufHdr->nTimeStamp, bufHdr->nFlags);
      if (mem_info->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
        mem_info->uFlag = 0;
        encoder->postEvent(OMX_EventBufferFlag, bufHdr->nOutputPortIndex,
            OMX_BUFFERFLAG_EOS);
      }
    }
  } else {
    OMX_LOGE("bd not be used");
    err = ERR_NOTIMPL;
    return err;
  }
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::prepare() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  err = createAmpVenc();
  CHECKAMPERR(err);
  AMP_COMPONENT_CONFIG config;
  kdMemset(&config, 0 ,sizeof(config));
  config._d = AMP_COMPONENT_VENC;
  if (mOutputPort->isTunneled()) {
    config._u.pADEC.mode = AMP_TUNNEL;
  } else {
    config._u.pADEC.mode = AMP_NON_TUNNEL;
  }
  OMX_VIDEO_PORTDEFINITIONTYPE &video_def = mInputPort->getVideoDefinition();
  config._u.pVENC.uiWidth = video_def.nFrameWidth;
  config._u.pVENC.uiHeight = video_def.nFrameHeight;
  config._u.pVENC.uiColorFmt = getAmpColorFormat(video_def.eColorFormat);
  assert(video_def.nFrameWidth != 0);
  assert(video_def.nFrameHeight != 0);
  OMX_VIDEO_PORTDEFINITIONTYPE &out_def = mOutputPort->getVideoDefinition();
  config._u.pVENC.uiCodingFmt = getAmpVideoCodecType(out_def.eCompressionFormat);
  config._u.pVENC.uiEncMode = AMP_VENC_MODE_EXPRESS;
  config._u.pVENC.uiInitQp = 27; //[0 ~ 100]
  config._u.pVENC.uiGopSize = 30;
  config._u.pVENC.uiGopType = AMP_RCGOP_IP;
  config._u.pVENC.uiH264Profile = 0; // VENC_PROFILE_MAIN
  config._u.pVENC.uiH264Level = 0; // VENC_LEVEL_4_0
  config._u.pVENC.uiFrameRateNum = mFramerateQ16;
  config._u.pVENC.uiFrameRateDen = 0x10000;
  config._u.pVENC.uiRateCtrlSel = 9;
  config._u.pVENC.uiVBVSize = 0;
  config._u.pVENC.uiHRDBitRate = 0;
  config._u.pVENC.uiAvcOn = 1;
  config._u.pVENC.uiAudOn = 0;
  OMX_LOGD("amp color format %d, frame_rate 0x%x",
      config._u.pVENC.uiColorFmt, config._u.pVENC.uiFrameRateNum);
  AMP_RPC(err, AMP_VENC_Open, mAmpHandle, &config);
  static_cast<OmxVideoEncoderPort *>(mInputPort)->setAmpHandle(mAmpHandle);
  static_cast<OmxVideoEncoderPort *>(mOutputPort)->setAmpHandle(mAmpHandle);

  if (!mInputPort->isTunneled()) {
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_INPUT, 0, bufferCb, this);
    CHECKAMPERR(err);
  }
  if (!mOutputPort->isTunneled()) {
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_OUTPUT, 0, bufferCb, this);
    CHECKAMPERR(err);
  }
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::release() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  if (!mInputPort->isTunneled()) {
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_INPUT, 0, bufferCb);
    CHECKAMPERR(err);
  }
  if (!mOutputPort->isTunneled()) {
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_OUTPUT, 0, bufferCb);
    CHECKAMPERR(err);
  }
  AMP_RPC(err, AMP_VENC_Close, mAmpHandle);
  CHECKAMPERR(err);
  err = destroyAmpVenc();
  CHECKAMPERR(err);
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::preroll() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxVideoEncoder::pause() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VENC_SetState, mAmpHandle, AMP_PAUSED);
  CHECKAMPERR(err);
  mAmpState = AMP_PAUSED;
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::resume() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VENC_SetState, mAmpHandle, AMP_EXECUTING);
  CHECKAMPERR(err);
  mAmpState = AMP_EXECUTING;
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::start() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  err = registerBds(static_cast<OmxVideoEncoderPort *>(mInputPort));
  CHECKAMPERR(err);
  err = registerBds(static_cast<OmxVideoEncoderPort *>(mOutputPort));
  CHECKAMPERR(err);
  AMP_RPC(err, AMP_VENC_SetState, mAmpHandle, AMP_EXECUTING);
  CHECKAMPERR(err);
  mAmpState = AMP_EXECUTING;
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::stop() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VENC_SetState, mAmpHandle, AMP_IDLE);
  CHECKAMPERR(err);
  mAmpState = AMP_IDLE;
  clearPort();
  err = unregisterBds(static_cast<OmxVideoEncoderPort *>(mInputPort));
  CHECKAMPERR(err);
  err = unregisterBds(static_cast<OmxVideoEncoderPort *>(mOutputPort));
  CHECKAMPERR(err);
  mEOS = OMX_FALSE;
  mOutputEOS = OMX_FALSE;
  mTsRecover.clear();
  VENC_TRACE("Exit");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::flush() {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VENC_SetState, mAmpHandle, AMP_IDLE);
  CHECKAMPERR(err);
  clearPort();
  AMP_RPC(err, AMP_VENC_SetState, mAmpHandle, mAmpState);
  VENC_TRACE("EXIT");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::registerBds(OmxVideoEncoderPort *port) {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  UINT32 uiPortIdx = 0;
  if (port->isInput()) {
    for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
      AmpBuffer *amp_buffer = port->getAmpBuffer(i);
      if (NULL != amp_buffer) {
        amp_buffer->addMemInfoTag();
        amp_buffer->addFrameInfoTag();
        AMP_RPC(err, AMP_VENC_RegisterBD, mAmpHandle, AMP_PORT_INPUT,
            uiPortIdx, amp_buffer->getBD());
        CHECKAMPERRLOG(err, "register BDs at VENC input port");
      }
    }
  } else {
    for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
      AmpBuffer *amp_buffer = port->getAmpBuffer(i);
      if (NULL != amp_buffer) {
        amp_buffer->addMemInfoTag();
        amp_buffer->addStreamInfoTag();
        AMP_RPC(err, AMP_VENC_RegisterBD, mAmpHandle, AMP_PORT_OUTPUT,
            uiPortIdx, amp_buffer->getBD());
        CHECKAMPERRLOG(err, "regsiter BDs at VENC output port");
      }
    }
  }
  VENC_TRACE("EXIT");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::unregisterBds(OmxVideoEncoderPort *port) {
  VENC_TRACE("Enter");
  HRESULT err = SUCCESS;
  AMP_PORT_IO portIo;
  if (port->isInput()) {
    portIo = AMP_PORT_INPUT;
  } else {
    portIo = AMP_PORT_OUTPUT;
  }
  AMP_BD_ST *bd;
  UINT32 uiPortIdx = 0;
  for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
    bd = port->getBD(i);
    if (NULL != bd) {
      AMP_RPC(err, AMP_VENC_UnregisterBD, mAmpHandle, portIo, uiPortIdx, bd);
      CHECKAMPERRLOG(err, "regsiter BDs at VENC output port");
    }
  }
  VENC_TRACE("EXIT");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::pushAmpBd(AMP_PORT_IO port,
    UINT32 portindex, AMP_BD_ST *bd) {
  OMX_LOGV("(%s) pushAmpBd, bd 0x%x, bdid 0x%x, allocva 0x%x",
      port == AMP_PORT_INPUT ? "In":"Out", bd, bd->uiBDId, bd->uiAllocVA);
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_VENC_PushBD, mAmpHandle, port, portindex, bd);
  CHECKAMPERRLOG(err, "push BD to VENC");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxVideoEncoder::componentDeInit(OMX_HANDLETYPE hComponent) {
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxVideoEncoder *processor = static_cast<OmxVideoEncoder *>(comp);
  delete processor;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoEncoder::createComponent(
  OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxVideoEncoder* comp = new OmxVideoEncoder(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}
}  // namespace berlin
