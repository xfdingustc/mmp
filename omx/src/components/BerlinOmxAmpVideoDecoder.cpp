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

#define LOG_TAG "BerlinOmxAmpVideoDecoder"
#include <BerlinOmxAmpVideoDecoder.h>
extern "C" {
#include <amp_event_listener.h>
#include <amp_video_api.h>
#include <amp_vdec_userdata.h>
}
#include <BerlinOmxParseXmlCfg.h>

//NOTICE: keep these values same with vdec_api.h
#define VDEC_MPEG2_PROFILE_SIMPLE 5
#define VDEC_MPEG2_PROFILE_MAIN 4

#define HAL_PIXEL_FORMAT_UYVY 0x100

//use overlay surface to display the buffer.
//auto allocate ION memory when use overlay.
#define GRALLOC_USAGE_VIDEO_OVERLAY 0x01000000

//disable it if we want to use secure TZ solution.
#define GFX_USE_OVERLAY

namespace berlin {

static const unsigned int kWidthAlignment = 64;
static const unsigned int kHeightAlignment = 32;
static const int kFrameInModeMask = 0x0200;
static const int kStreamInModeMask = 0x0000;
static const int kYV12OutputMask = 0x0400;
static const int kDisplayCopyframeMask = 0x1000;
static const int kStreamPositionMask = 0x1fffffff;
static const int kMaxFrameRate = 60;
static const int kLowLatencyModeMask = 0x0040;
static const char kUuidWidevine[] = UUID_WIDEVINE;
static const char kUuidPlayReady[] = UUID_PLAYREADY;
static const char kUuidWMDRM[] = UUID_WMDRM;
static const char kUuidMarLin[] = UUID_MARLIN;
static const unsigned int kPlayReadyType = 1;
static const unsigned int kWideVineType = 2;
static const unsigned int kWMDRMType = 3;
static const unsigned int kMarLinType = 4;
static const unsigned int tsp_vdhub_stream_type_playready = 11;
static const unsigned int tsp_vdhub_stream_type_widevine = 13;
static const unsigned int es_buf_size = 8 * 1024 * 1024;
static const unsigned int es_buf_pad_size = 4 * 1024;
static const unsigned int ctrl_buf_num = 2000;
static const unsigned int stream_in_buf_size = 8 * 1024 * 1024;
static const unsigned int stream_in_max_push_size = 2 * 1024 * 1024 - 128;
static const unsigned int stream_in_bd_num = 128;
static const unsigned int stream_in_align_size = 4 * 1024;
static const unsigned int stream_in_eos_padding_size = 64 * 1024;
static const unsigned int stream_in_max_pts_tag_num = 12;
static const unsigned int frame_in_buf_size = 8 * 1024 * 1024;
static const unsigned int frame_in_bd_num = 128;
static const unsigned int frame_in_align_size = 0;
static const unsigned int frame_in_pad_size = 1024 * 1024 * 2;
static const unsigned int eos_padding[] =
{
    0xff010000, 0x04000080,
    0x88888800, 0x88888888,
    0x88888888, 0x88888888,
    0x88888888, 0x88888888,
};

static const VideoFormatType kColorSupport[] = {
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYUV420Planar, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatCrYCbY, 15 <<16},
  {OMX_VIDEO_CodingUnused, OMX_COLOR_FormatYVU420Planar, 15 <<16},
};

static OMX_BOOL CheckMediaHelperEnable(
    OMX_BOOL mEnable, char const *pStr) {
  MEDIA_S8 value[MEDIA_VALUE_LENGTH];
  kdMemset(value, 0x0, MEDIA_VALUE_LENGTH);
  if ((mEnable == OMX_TRUE) && mediainfo_get_property(
      reinterpret_cast<const MEDIA_S8 *>(pStr), value) &&
      kdMemcmp(value, "1", 1) == 0) {
    return OMX_TRUE;
  } else {
    return OMX_FALSE;
  }
}

static int32_t mapStereoMode(int32_t convert_mode_3d) {
    switch(convert_mode_3d) {
        case VDEC_BUF_3D_FMT_FP:  return marvell::kStereoFramePacking;
        case VDEC_BUF_3D_FMT_SBS: return marvell::kStereoSBS;
        case VDEC_BUF_3D_FMT_TAB: return marvell::kStereoTB;
        case VDEC_BUF_3D_FMT_FS:  return marvell::kStereoFrameSequential;
        default: return marvell::kStereoNone;
    }
}

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
      stride = (stride + 63) & (~63);
      height = (height + 31) & (~31);
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
      return (width + 63) & (~63);
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
      return (height + 31) & (~31);
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_UYVY:
      return (height + 15) & (~15);
    default:
      return height;
  }
}
#endif

OmxAmpVideoDecoder::OmxAmpVideoDecoder() {
}

OmxAmpVideoDecoder::OmxAmpVideoDecoder(OMX_STRING name) :
    mEOS(OMX_FALSE),
    mThread(NULL),
    mThreadAttr(NULL),
    mPauseCond(NULL),
    mPauseLock(NULL),
    mBufferLock(NULL),
    mListener(NULL) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
  mMark.hMarkTargetComponent = NULL;
  mMark.pMarkData = NULL;
  mShouldExit = OMX_FALSE;
  mInputFrameNum = 0;
  mOutputFrameNum = 0;
  mInBDBackNum = 0;
  mOutBDPushNum = 0;
  mStreamPosition = 0;
  mInited = OMX_FALSE;
  mOutputEOS = OMX_FALSE;
  mPaused = OMX_FALSE;
  mProfileLevelPtr = NULL;
  mProfileLevelNum = 0;
  mExtraData = NULL;
  mSecureExtraData = NULL;
  mSourceControl = NULL;
  mObserver = NULL;
  mVideoPlaneId = -1;
  mSourceId = -1;
  mPool = NULL;
  mPushedBdNum = 0;
  mReturnedBdNum = 0;
  mAddedPtsTagNum = 0;
  mCachedhead = NULL;
  mCryptoInfo = NULL;
  mMediaHelper = OMX_FALSE;
  mDMXHandle = NULL;
  mSchemeIdSend = OMX_FALSE;
  mDataCount = 0;
  mIsWMDRM = OMX_FALSE;
  mShmHandle = 0;
  mAdaptivePlayback.bEnable = OMX_FALSE;
  mAdaptivePlayback.bMaxWidth = 0;
  mAdaptivePlayback.bMaxHeight = 0;
  mStartPushOutBuf = OMX_FALSE;
  mHasRegisterOutBuf = OMX_FALSE;
  mTsRecover = NULL;
  mSar = 0.0f;
  mPar = 0.0f;
  mDar = 0.0f;
  mAFD = 0;
  mVideoPresence = OMX_VIDEO_PRESENCE_FALSE;
  mDispHandle = NULL;
}

OmxAmpVideoDecoder::~OmxAmpVideoDecoder() {
  destroyAmpVideoDecoder();
  OMX_LOGD("destroyed");
}

OMX_ERRORTYPE OmxAmpVideoDecoder::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char *role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_AVC_SECURE, 24)) {
    addRole(OMX_ROLE_VIDEO_DECODER_AVC_SECURE);
#if PLATFORM_SDK_VERSION >= 19
    mSecure = OMX_TRUE;
#else
    kdMemcpy(mDrm.nUUID, kUuidWidevine, sizeof(mDrm.nUUID));
#endif
  } else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_AVC, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_AVC);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_H263, 18))
    addRole(OMX_ROLE_VIDEO_DECODER_H263);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MPEG2, 19))
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG2);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MPEG4, 19))
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG4);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_WMV_SECURE, 24))
    addRole(OMX_ROLE_VIDEO_DECODER_WMV_SECURE);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_WMV, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_WMV);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MJPEG, 19))
    addRole(OMX_ROLE_VIDEO_DECODER_MJPEG);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_RV, 16))
    addRole(OMX_ROLE_VIDEO_DECODER_RV);
#ifdef _ANDROID_
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VPX, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_VPX);
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VP8, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_VP8);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VC1_SECURE, 24))
    addRole(OMX_ROLE_VIDEO_DECODER_VC1_SECURE);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_VC1, 17))
    addRole(OMX_ROLE_VIDEO_DECODER_VC1);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_MSMPEG4, 21))
    addRole(OMX_ROLE_VIDEO_DECODER_MSMPEG4);
  else if (!strncmp(role_name, OMX_ROLE_VIDEO_DECODER_HEVC, 18))
    addRole(OMX_ROLE_VIDEO_DECODER_HEVC);
#endif
  else if (!strncmp(role_name, "video_decoder", 13)) {
    addRole(OMX_ROLE_VIDEO_DECODER_AVC_SECURE);
    addRole(OMX_ROLE_VIDEO_DECODER_AVC);
    addRole(OMX_ROLE_VIDEO_DECODER_H263);
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG2);
    addRole(OMX_ROLE_VIDEO_DECODER_MPEG4);
    addRole(OMX_ROLE_VIDEO_DECODER_WMV_SECURE);
    addRole(OMX_ROLE_VIDEO_DECODER_WMV);
    addRole(OMX_ROLE_VIDEO_DECODER_MJPEG);
    addRole(OMX_ROLE_VIDEO_DECODER_RV);
#ifdef _ANDROID_
    addRole(OMX_ROLE_VIDEO_DECODER_VPX);
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    addRole(OMX_ROLE_VIDEO_DECODER_VP8);
    addRole(OMX_ROLE_VIDEO_DECODER_VC1_SECURE);
    addRole(OMX_ROLE_VIDEO_DECODER_VC1);
    addRole(OMX_ROLE_VIDEO_DECODER_MSMPEG4);
    addRole(OMX_ROLE_VIDEO_DECODER_HEVC);
#endif
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_AVC, 17)) {
    mInputPort = new  OmxAmpAvcPort(kVideoPortStartNumber, OMX_DirInput);
    mProfileLevelPtr = kAvcProfileLevel;
    mProfileLevelNum = NELM(kAvcProfileLevel);
  } else if  (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_H263, 18)) {
    mInputPort = new  OmxAmpH263Port(kVideoPortStartNumber, OMX_DirInput);
    mProfileLevelPtr = kH263ProfileLevel;
    mProfileLevelNum = NELM(kH263ProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_MPEG2, 19)) {
    mInputPort = new  OmxAmpMpeg2Port(kVideoPortStartNumber, OMX_DirInput);
    mProfileLevelPtr = kMpeg2ProfileLevel;
    mProfileLevelNum = NELM(kMpeg2ProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_MPEG4, 19)) {
    mInputPort = new  OmxAmpMpeg4Port(kVideoPortStartNumber, OMX_DirInput);
    mProfileLevelPtr = kMpeg4ProfileLevel;
    mProfileLevelNum = NELM(kMpeg4ProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_WMV, 17)) {
    mInputPort = new  OmxAmpWMVPort(kVideoPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_MJPEG, 19)) {
    mInputPort = new   OmxAmpMjpegPort(kVideoPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_RV, 16)) {
    mInputPort = new OmxAmpRVPort(kVideoPortStartNumber, OMX_DirInput);
#ifdef _ANDROID_
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_VPX, 17)) {
    mInputPort = new  OmxAmpVP8Port(kVideoPortStartNumber, OMX_DirInput);
    mProfileLevelPtr = kVp8ProfileLevel;
    mProfileLevelNum = NELM(kVp8ProfileLevel);
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_VP8, 17)) {
    mInputPort = new  OmxAmpVP8Port(kVideoPortStartNumber, OMX_DirInput);
    mProfileLevelPtr = kVp8ProfileLevel;
    mProfileLevelNum = NELM(kVp8ProfileLevel);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_VC1, 17)) {
    mInputPort = new  OmxAmpVC1Port(kVideoPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_MSMPEG4, 21)) {
    mInputPort = new  OmxAmpMsMpeg4Port(kVideoPortStartNumber, OMX_DirInput);
  }  else if (!strncmp(mActiveRole, OMX_ROLE_VIDEO_DECODER_HEVC, 18)) {
    mInputPort = new  OmxAmpHevcPort(kVideoPortStartNumber, OMX_DirInput);
#endif
  } else {
    mInputPort = new OmxAmpVideoPort(kVideoPortStartNumber, OMX_DirInput);
  }
  addPort(mInputPort);
  mOutputPort = new OmxAmpVideoPort(kVideoPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);
  mUseNativeBuffers = OMX_FALSE;
  OMX_PARAM_PORTDEFINITIONTYPE def;
  mInputPort->getDefinition(&def);
  def.nBufferCountActual = 4;
  def.nBufferSize = 1024 * 1024 * 2;
  mInputPort->setDefinition(&def);
  mOutputPort->getDefinition(&def);
  def.nBufferSize = 1920 * 1088 * 3 / 2;
  def.nBufferCountActual = 4;
  def.nBufferAlignment = 64;
  def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  mOutputPort->setDefinition(&def);
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::componentDeInit(OMX_HANDLETYPE
    hComponent) {
  OMX_LOG_FUNCTION_ENTER;
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxAmpVideoDecoder *vdec = static_cast<OmxAmpVideoDecoder *>(comp);
  delete vdec;
  reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate = NULL;
  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

HRESULT OmxAmpVideoDecoder::vdecUserDataCb(
        AMP_COMPONENT component,
        AMP_PORT_IO ePortIo,
        UINT32 uiPortIdx,
        struct AMP_BD_ST *hBD,
        void *context) {
    AMP_BDTAG_MEMINFO *pMemInfo;
    HRESULT result = SUCCESS;
    uint32_t afdIdentifier = 0;
    uint8_t activeFmtFlag = 0;

    USER_DATA_BLOCK_HEADER *pHeader;
    UINT8 *pVirtAddr;
    int newAfd = 0;
    OmxAmpVideoDecoder *decoder = static_cast<OmxAmpVideoDecoder *>(context);

    result = AMP_BDTAG_GetWithIndex(hBD, 0, (void **)&pMemInfo);
    CHECKAMPERRLOG(result, "AMP_BDTAG_GetWithIndex()");

    result = AMP_SHM_Ref(pMemInfo->uMemHandle, (void **)(&pVirtAddr));
    CHECKAMPERRLOG(result, "AMP_SHM_Ref()");

    pVirtAddr += pMemInfo->uMemOffset;
    pHeader = (USER_DATA_BLOCK_HEADER *) pVirtAddr;
    OMX_LOGD("Userdata type %d, payload length %d\n", pHeader->m_type, pHeader->m_length);

    if (pHeader->m_type == VIDEO_USER_DATA_TYPE_AFD) {
      if (decoder->mInputPort->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
        //Active Format Description for MPEG-2 video
        //              afd_identifier                        (32-bit, value is 0x44544731)
        //                   "0"                                             (1-bit)
        //           active_format_flag                                (1-bit)
        //       reserved (set to "00 0001")                         (6-bit)
        //    if (active_format_flag == 1) {
        //        reserved (set to "1111" )                            (4-bit)
        //        active_format                                            (4-bit)
        //    }
        afdIdentifier += *(((UINT8 *)pVirtAddr + pHeader->m_body_offset)) << 24;
        afdIdentifier += *(((UINT8 *)pVirtAddr + pHeader->m_body_offset + 1)) << 16;
        afdIdentifier += *(((UINT8 *)pVirtAddr + pHeader->m_body_offset + 2)) << 8;
        afdIdentifier += *(((UINT8 *)pVirtAddr + pHeader->m_body_offset + 3));
        activeFmtFlag = *((UINT8 *)pVirtAddr + pHeader->m_body_offset + 4) & 0x40;
        if ((afdIdentifier == 0x44544731) && activeFmtFlag == 0x40) {
          newAfd = (*((UINT8 *)pVirtAddr + pHeader->m_body_offset + 5)) & 0x0f;
        } else {
          OMX_LOGW("unknown user data structure or no active format info, "
              "afd_identifier(0x%x), active_format_flag(0x%x)", afdIdentifier, activeFmtFlag);
        }
      }
      if (decoder->mInputPort->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC) {
        //Active Format Description for H264/AVC video
        //    itu_t_t35_country_code                        (8-bit)
        //    itu_t_t35_provider_code                      (16-bit)
        //        afd_identifier                     (32-bit, value is 0x44544731)
        //               "0"                                          (1-bit)
        //     active_format_flag                              (1-bit)
        //        alignment_bits                      (6-bit, value is "00 0001")
        //    if (active_format_flag == 1) {
        //        reserved                                  (4-bit, value is "1111")
        //        active_format                                  (4-bit)
        //    }
        afdIdentifier += *((UINT8 *)pVirtAddr + pHeader->m_body_offset + 3) << 24;
        afdIdentifier += *((UINT8 *)pVirtAddr + pHeader->m_body_offset + 4) << 16;
        afdIdentifier += *((UINT8 *)pVirtAddr + pHeader->m_body_offset + 5) << 8;
        afdIdentifier += *((UINT8 *)pVirtAddr + pHeader->m_body_offset + 6);
        activeFmtFlag = *((UINT8 *)pVirtAddr + pHeader->m_body_offset + 7) & 0x40;
        if ((afdIdentifier == 0x44544731) && activeFmtFlag == 0x40) {
          newAfd = (*((UINT8 *)pVirtAddr + pHeader->m_body_offset + 8)) & 0x0f;
        } else {
          OMX_LOGW("unknown user data structure or no active format info, "
              "afd_identifier(0x%x), active_format_flag(0x%x)", afdIdentifier, activeFmtFlag);
        }
      }
      OMX_LOGD("AFD value is %d", newAfd);
      if (newAfd != decoder->mAFD) {
        decoder->mAFD = newAfd;
        decoder->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoAFDChanged), newAfd, 0);
      }
    }

    result = AMP_SHM_Unref(pMemInfo->uMemHandle);
    CHECKAMPERRLOG(result, "AMP_SHM_Unref()");

    // output port 1 is for user data CC/AFD
    AMP_RPC(result, AMP_VDEC_PushBD, decoder->mAmpHandle, AMP_PORT_OUTPUT, 1, hBD);
    CHECKAMPERRLOG(result, "AMP_VDEC_PushBD()");

    return SUCCESS;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::getConfig(
    OMX_INDEXTYPE index, OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  switch (index) {
    case OMX_IndexConfigVideoProperties: {
      OMX_VIDEO_PROPERTIES *vconfig =
        reinterpret_cast<OMX_VIDEO_PROPERTIES *>(config);

      vconfig->width = mVdecInfo.nMaxWidth;
      vconfig->height = mVdecInfo.nMaxHeight;
      vconfig->framerate = mInputPort->getVideoDefinition().xFramerate;
      if (mVdecInfo.nFrameRate != 0) {
        vconfig->framerate = mVdecInfo.nFrameRate;
      }

      if (mVdecInfo.nIsIntlSeq == true) {
        vconfig->scanType = OMX_VIDEO_SCAN_TYPE_INTERLACED;
      } else {
        vconfig->scanType = OMX_VIDEO_SCAN_TYPE_PROGRESSIVE;
      }

      // DAR = SAR * PAR
      if (mVdecInfo.nMaxHeight != 0 && mVdecInfo.nMaxWidth != 0) {
        mSar= (float)mVdecInfo.nMaxWidth / mVdecInfo.nMaxHeight;
      }
      if (mVdecInfo.nSarWidth != 0 && mVdecInfo.nSarHeight != 0) {
        mPar = (float)mVdecInfo.nSarWidth / mVdecInfo.nSarHeight;
      }
      mDar = mSar * mPar;

      if (fabs(mDar - 1.0f) < 0.001f) {
        vconfig->aspectRatio = OMX_VIDEO_ASPECT_RATIO_1_1;
      } else if (fabs(mDar - 1.333f) < 0.001f) {
        vconfig->aspectRatio = OMX_VIDEO_ASPECT_RATIO_4_3;
      } else if (fabs(mDar - 1.777f) < 0.001f) {
        vconfig->aspectRatio = OMX_VIDEO_ASPECT_RATIO_16_9;
      } else {
        vconfig->aspectRatio = OMX_VIDEO_ASPECT_RATIO_UNKNOWN;
      }

      break;
    }
    case OMX_IndexConfigVideoPixelAspectRatio: {
      float *par = reinterpret_cast<float *>(config);
      if (mVdecInfo.nSarWidth != 0 && mVdecInfo.nSarHeight != 0) {
        mPar = (float)mVdecInfo.nSarWidth / mVdecInfo.nSarHeight;
      } else {
        mPar = 1.0f;
      }
      *par = mPar;

      break;
    }
    case OMX_IndexConfigVideoAFD: {
      int *afd = reinterpret_cast<int *>(config);
      *afd = mAFD;
      break;
    }
    case OMX_IndexConfigVideo3DFormat: {
      OMX_VIDEO_3D_FORMAT *fmt3D = reinterpret_cast<OMX_VIDEO_3D_FORMAT *>(config);
      *fmt3D = static_cast<OMX_VIDEO_3D_FORMAT>(mVdecInfo.nStereoMode);
      break;
    }
    default: {
      err = OMX_ErrorUnsupportedIndex;
      break;
    }
  }

  return err;
}


OMX_ERRORTYPE OmxAmpVideoDecoder::getParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  HRESULT ret = SUCCESS;
  UINT32 profile = 0;
  OMX_LOGD("getParameter index 0x%x",index);
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
        if (video_param->nIndex >= NELM(kColorSupport)) {
          err = OMX_ErrorNoMore;
          break;
        }
        const VideoFormatType *v_fmt = &kColorSupport[video_param->nIndex];
        video_param->eCompressionFormat = v_fmt->eCompressionFormat;
        video_param->eColorFormat = v_fmt->eColorFormat;
        video_param->xFramerate = v_fmt->xFramerate;
      }
      break;
    }
    case OMX_IndexParamVideoProfileLevelQuerySupported:  {
      OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE *>(params);
      // TODO: keep compatible with 1.1 spec
      /*err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }*/
      OmxPortImpl *port = getPort(prof_lvl->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
        OMX_U32 idx = prof_lvl->nIndex;
#else
        OMX_U32 idx = prof_lvl->nProfileIndex;
#endif
        if (NULL != mProfileLevelPtr) {
          if (idx >= mProfileLevelNum){
            err = OMX_ErrorNoMore;
          } else {
            prof_lvl->eProfile = mProfileLevelPtr[idx].eProfile;
            prof_lvl->eLevel= mProfileLevelPtr[idx].eLevel;
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
      // TODO: keep compatible with 1.1 spec
      /*err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }*/
      OmxPortImpl *port = getPort(prof_lvl->nPortIndex);
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
        } else if (port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
          if (mAmpHandle) {
            AMP_RPC(ret, AMP_VDEC_GetCmdP1, mAmpHandle, AMP_VDEC_CMD_GET_PROFILE, &profile);
            CHECKAMPERRLOG(ret, "get MPEG2 profile.");
            if (profile == VDEC_MPEG2_PROFILE_SIMPLE) {
              OMX_LOGD("MPEG2 simple profile");
              prof_lvl->eProfile = kMpeg2ProfileLevel[0].eProfile;
              prof_lvl->eLevel = kMpeg2ProfileLevel[0].eLevel;
            } else if (profile == VDEC_MPEG2_PROFILE_MAIN) {
              OMX_LOGD("MPEG2 main profile");
              prof_lvl->eProfile = kMpeg2ProfileLevel[1].eProfile;
              prof_lvl->eLevel = kMpeg2ProfileLevel[1].eLevel;
            } else {
              OMX_LOGE("unknown profile info from vdec");
              prof_lvl->eProfile = OMX_VIDEO_MPEG2ProfileUnknown;
              prof_lvl->eLevel= OMX_VIDEO_MPEG2LevelUnknown;
            }
          } else {
            prof_lvl->eProfile = OMX_VIDEO_MPEG2ProfileUnknown;
            prof_lvl->eLevel= OMX_VIDEO_MPEG2LevelUnknown;
          }
        }
      }
      break;
    }
    case OMX_IndexParamVideoAvc: {
      OMX_VIDEO_PARAM_AVCTYPE *codec_param =
          reinterpret_cast<OMX_VIDEO_PARAM_AVCTYPE *>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo &&
          port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC)
        *codec_param = ((OmxAmpAvcPort *)port)->getCodecParam();
      break;
    }
    case OMX_IndexParamVideoRv: {
      OMX_VIDEO_PARAM_RVTYPE *codec_param =
          reinterpret_cast<OMX_VIDEO_PARAM_RVTYPE *>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo &&
          port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingRV) {
        *codec_param = ((OmxAmpRVPort *)port)->getCodecParam();
      }
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
      if (mInputPort->mIsDrm
          || mInputPort->mOmxDisableTvp) {
#ifdef GFX_USE_OVERLAY
        // We can remove the HW_RENDER | PRIVATE_0 after GFX side auto using
        // ION memory for overlay. And we also need to add PROTECTED usage to
        // let GFX allocate secure memory to protect the frame buffer.
        OMX_LOGD("GetNativeBufferUsage use Overlay.");
        getUsage_nativebuffer->nUsage = GRALLOC_USAGE_VIDEO_OVERLAY
            | GRALLOC_USAGE_PROTECTED
            | GraphicBuffer::USAGE_HW_RENDER
            | GRALLOC_USAGE_PRIVATE_0;
#else
        // We can use secure TZ solution once GFX side can support it.
        OMX_LOGD("GetNativeBufferUsage use Secure TZ solution.");
        getUsage_nativebuffer->nUsage = GraphicBuffer::USAGE_HW_RENDER
            | GRALLOC_USAGE_PRIVATE_0
            | GRALLOC_USAGE_PROTECTED
#endif
      } else {
        //it will auto allocate ION memory without 512k checking.
        OMX_LOGD("GetNativeBufferUsage use Gfx HW Render.");
        getUsage_nativebuffer->nUsage = GraphicBuffer::USAGE_HW_RENDER
            | GRALLOC_USAGE_PRIVATE_0;
      }
      OMX_LOGD("GetNativeBufferUsage %lu", getUsage_nativebuffer->nUsage);
      break;
    }
#endif

#ifdef OMX_IndexExt_h
    case OMX_IndexExtVideoHeadBitrate: {
      OMX_U32* val = reinterpret_cast<OMX_U32 *>(params);
      UINT32 tmp = 0;
      AMP_RPC(ret, AMP_VDEC_GetCmdP1, mAmpHandle, AMP_VDEC_CMD_GET_HEAD_BITRATE, &tmp);
      CHECKAMPERRLOGNOTRETURN(ret, "GET_HEAD_BITRATE");
      *val = tmp;
      break;
    }
    case OMX_IndexExtVideoMediaType: {
      OMX_U32* val = reinterpret_cast<OMX_U32*>(params);
      UINT32 tmp = 0;
      AMP_RPC(ret, AMP_VDEC_GetCmdP1, mAmpHandle, AMP_VDEC_CMD_GET_MEDIA_TYPE, &tmp);
      CHECKAMPERRLOGNOTRETURN(ret, "GET_MEDIA_TYPE");
      *val = tmp;
      break;
    }
    case OMX_IndexExtVideoProfile: {
      OMX_U32* val = reinterpret_cast<OMX_U32*>(params);
      UINT32 tmp = 0;
      AMP_RPC(ret, AMP_VDEC_GetCmdP1, mAmpHandle, AMP_VDEC_CMD_GET_PROFILE, &tmp);
      CHECKAMPERRLOGNOTRETURN(ret, "GET_PROFILE");
      *val = tmp;
      break;
    }
    case OMX_IndexExtVideoGopBitrate: {
      OMX_U32* val = reinterpret_cast<OMX_U32*>(params);
      UINT32 tmp = 0;
      AMP_RPC(ret, AMP_VDEC_GetCmdP1, mAmpHandle, AMP_VDEC_CMD_GET_GOP_BITRATE, &tmp);
      CHECKAMPERRLOGNOTRETURN(ret, "GET_GOP_BITRATE");
      *val = tmp;
      break;
    }
    case OMX_IndexExtVideoGopQP: {
      OMX_U32* val = reinterpret_cast<OMX_U32*>(params);
      UINT32 tmp = 0;
      AMP_RPC(ret, AMP_VDEC_GetCmdP1, mAmpHandle, AMP_VDEC_CMD_GET_GOP_QP, &tmp);
      CHECKAMPERRLOGNOTRETURN(ret, "GET_GOP_QP");
      *val = tmp;
      break;
    }
#endif

    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::setParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("setParameter index 0x%x",index);
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
      OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl =
          reinterpret_cast<OMX_VIDEO_PARAM_PROFILELEVELTYPE *>(params);
      // TODO: keep compatible with 1.1 spec
      /*err = CheckOmxHeader(prof_lvl);
      if (OMX_ErrorNone != err) {
        break;
      }*/
      OmxPortImpl *port = getPort(prof_lvl->nPortIndex);
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
      OMX_U32 maxstride = 0, maxheight = 0;
      OMX_COLOR_FORMATTYPE eFormat;
      OMX_VIDEO_PORTDEFINITIONTYPE &video_def = port->getVideoDefinition();
      stride = video_def.nFrameWidth;
      height = video_def.nFrameHeight;
      // eFormat = static_cast<OMX_COLOR_FORMATTYPE>(HAL_PIXEL_FORMAT_YV12);
      eFormat = static_cast<OMX_COLOR_FORMATTYPE>(HAL_PIXEL_FORMAT_UYVY);
      stride = getAlignedStride(eFormat, stride);
      height = getAlignedStride(eFormat, height);
      video_def.nFrameWidth = stride;
      video_def.nFrameHeight = height;
      size = getNativeBufferSize(eFormat, stride, height);
      if (mAdaptivePlayback.bEnable && mAdaptivePlayback.bMaxWidth > 0
          && mAdaptivePlayback.bMaxHeight > 0) {
        maxstride = getAlignedStride(eFormat, mAdaptivePlayback.bMaxWidth);
        maxheight = getAlignedStride(eFormat, mAdaptivePlayback.bMaxHeight);
        size = getNativeBufferSize(eFormat, maxstride, maxheight);
        // For resolution change case, we suppose to use the max display buf num.
        port->setBufferCount(19);
      }
      port->setBufferSize(size);
      port->updateDomainParameter();
      video_def.eColorFormat = eFormat;
      if (stride <= 176 && height <= 144 && maxstride <= 176 && maxheight <= 144) {
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
      android::UseAndroidNativeBufferParams *use_nativebuffer =
          reinterpret_cast<android::UseAndroidNativeBufferParams *>(params);
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
      OmxPortImpl *port = getPort(use_nativebuffer->nPortIndex);
      if (!port) {
        err = OMX_ErrorUndefined;
        OMX_LOGE("UseNativeBuffer error %d", err);
        break;
      }
      OMX_BUFFERHEADERTYPE **bufHdr = use_nativebuffer->bufferHeader;
      // port->mUseNativeBuffer = OMX_TRUE;
      GraphicBuffer *gfx = static_cast<GraphicBuffer *>(
          use_nativebuffer->nativeBuffer.get());
      uint32_t usage = GraphicBuffer::USAGE_HW_RENDER | GRALLOC_USAGE_PRIVATE_0;
      void *gid = NULL;
      gfx->lock(usage, &gid);
      err = useBuffer(
          bufHdr,
          use_nativebuffer->nPortIndex,
          use_nativebuffer->pAppPrivate,
          getNativeBufferSize(gfx->format, gfx->getStride(), gfx->getHeight()),
          static_cast<OMX_U8 *>(gid));
      static_cast<AmpBuffer *>((*bufHdr)->pPlatformPrivate)->setGfx((void *)gfx);
      gfx->unlock();
      break;
    }
#if PLATFORM_SDK_VERSION >= 19
    case OMX_IndexAndroidStoreMetadataInBuffers: {
      OMX_LOGD("StoreMetadataInBuffers");
      android::StoreMetaDataInBuffersParams *storeParams =
          reinterpret_cast<android::StoreMetaDataInBuffersParams *>(params);
      if (storeParams->nPortIndex != 1) {
        OMX_LOGE("StoreMetadataInBuffersParams port index is not 1");
        err = OMX_ErrorBadPortIndex;
        break;
      }
      mOutputPort->mStoreMetaDataInBuffers = storeParams->bStoreMetaData;
      break;
    }
    case OMX_IndexAndroidPrepareForAdaptivePlayback: {
      OMX_LOGD("PrepareForAdaptivePlayback");
      android::PrepareForAdaptivePlaybackParams *adaptive_playback =
          reinterpret_cast<android::PrepareForAdaptivePlaybackParams *>(params);
      err = CheckOmxHeader(adaptive_playback);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("UseNativeBuffer error %d", err);
        break;
      }
      if (adaptive_playback->nPortIndex < 1) {
        err = OMX_ErrorBadPortIndex;
        OMX_LOGE("Prepare for adaptive playback supported only for output port.");
        break;
      }
      if (adaptive_playback->bEnable && adaptive_playback->nMaxFrameWidth > 0 &&
          adaptive_playback->nMaxFrameHeight > 0) {
        OMX_LOGD("Enable adaptive video playback, MaxWidth %d MaxHeight %d",
          adaptive_playback->nMaxFrameWidth, adaptive_playback->nMaxFrameHeight);
        mAdaptivePlayback.bEnable = adaptive_playback->bEnable;
        mAdaptivePlayback.bMaxWidth = adaptive_playback->nMaxFrameWidth;
        mAdaptivePlayback.bMaxHeight = adaptive_playback->nMaxFrameHeight;
      } else {
        OMX_LOGD("Disable adaptive video playback.");
      }
      break;
    }
#endif
#endif
    case OMX_IndexParamVideoAvc: {
      OMX_VIDEO_PARAM_AVCTYPE *codec_param =
          reinterpret_cast<OMX_VIDEO_PARAM_AVCTYPE *>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo &&
          port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingAVC)
        static_cast<OmxAmpAvcPort *>(port)->setCodecParam(codec_param);
      break;
    }
    case OMX_IndexParamVideoRv: {
      OMX_VIDEO_PARAM_RVTYPE *codec_param =
          reinterpret_cast<OMX_VIDEO_PARAM_RVTYPE *>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo &&
          port->getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingRV) {
        static_cast<OmxAmpRVPort *>(port)->setCodecParam(codec_param);
      }
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

OMX_ERRORTYPE OmxAmpVideoDecoder::prepare() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  // mediahelper init
  if (mediainfo_buildup()) {
    mMediaHelper = OMX_TRUE;
    OMX_LOGD("Media helper build success");
  } else {
    OMX_LOGE("Media helper build fail");
  }
  if (mMediaHelper == OMX_TRUE) {
    // remove the dumped data
    mediainfo_remove_dumped_data(MEDIA_VIDEO);
  }

  err = getAmpVideoDecoder();
  CHECKAMPERRLOG(err, "get AMP video decoder");

  AMP_MEDIA_TYPE amp_codec = getAmpVideoCodecType(
      mInputPort->getVideoDefinition().eCompressionFormat);
  if (NULL == amp_codec) {
    OMX_LOGE("can not find the AMP vdec type.");
    return OMX_ErrorUndefined;
  }
  // Detect realvideo type.
  if ((OMX_PortDomainVideo == mInputPort->getDomain()) &&
      (OMX_VIDEO_CodingRV == mInputPort->getVideoDefinition().eCompressionFormat)) {
    OMX_VIDEO_PARAM_RVTYPE rvParams =
        static_cast<OmxAmpRVPort *>(mInputPort)->getCodecParam();
    if (OMX_VIDEO_RVFormat8 == rvParams.eFormat) {
      amp_codec = MEDIA_VES_RV30;
    } else if (OMX_VIDEO_RVFormat9 == rvParams.eFormat) {
      amp_codec = MEDIA_VES_RV40;
    }
  }
  OMX_LOGD("Amp video codec type is %s", AmpVideoCodec2String(amp_codec));

  AMP_COMPONENT_CONFIG config;
  kdMemset(&config, 0 ,sizeof(config));
  config._d = AMP_COMPONENT_VDEC;
  if (mOutputPort->isTunneled()) {
    OMX_LOGD("Use Tunnel Mode.");
    config._u.pVDEC.mode = AMP_SECURE_TUNNEL;
  } else {
    OMX_LOGD("Use Non-Tunnel Mode.");
    config._u.pVDEC.mode = AMP_NON_TUNNEL;
    mTsRecover = new TimeStampRecover();
  }
  config._u.pVDEC.uiType = amp_codec;
  config._u.pVDEC.uiWidth = 0;
  config._u.pVDEC.uiHeight = 0;
  config._u.pVDEC.uiFrameRate = 0;
  char kUuidNoDrm[sizeof(mDrm.nUUID)] = { 0 };
  AMP_SHM_HANDLE es_handle;
  if (0 == kdMemcmp(mDrm.nUUID, kUuidPlayReady, sizeof(mDrm.nUUID))) {
#ifndef DISABLE_PLAYREADY
    OMX_LOGD("Secure Playready Tvp");
#else
    OMX_LOGD("Playready is not supported.");
    return OMX_ErrorUndefined;
#endif
    configTvpVersion();
    mInputPort->configDrm(kPlayReadyType, &es_handle);
    if (mInputPort->mAmpDisableTvp == OMX_TRUE) {
      config._u.pVDEC.uiFlag = kStreamInModeMask;
    } else {
      config._u.pVDEC.uiFlag = kFrameInModeMask;
      config._u.pVDEC.uiSecureType = tsp_vdhub_stream_type_playready;
      config._u.pVDEC.uiSHMHandle = es_handle;
      config._u.pVDEC.uiSHMSize = es_buf_size;
      config._u.pVDEC.uiPadSize = es_buf_pad_size;
    }
  } else if (0 == kdMemcmp(mDrm.nUUID, kUuidWidevine, sizeof(mDrm.nUUID))) {
    OMX_LOGD("Secure Widevine Tvp");
    configTvpVersion();
    mInputPort->configDrm(kWideVineType, &es_handle);
    if (mInputPort->mAmpDisableTvp == OMX_TRUE) {
      config._u.pVDEC.uiFlag = kStreamInModeMask;
    } else {
      config._u.pVDEC.uiFlag = kFrameInModeMask;
      config._u.pVDEC.uiSecureType = tsp_vdhub_stream_type_widevine;
      config._u.pVDEC.uiSHMHandle = es_handle;
      config._u.pVDEC.uiSHMSize = es_buf_size;
      config._u.pVDEC.uiPadSize = es_buf_pad_size;
    }
  } else if (0 == kdMemcmp(mDrm.nUUID, kUuidWMDRM, sizeof(mDrm.nUUID))) {
#ifndef DISABLE_PLAYREADY
    OMX_LOGD("Secure WMDRM");
#else
    OMX_LOGD("WMDRM is not supported.");
    return OMX_ErrorUndefined;
#endif
    mIsWMDRM = OMX_TRUE;
    mInputPort->mDrmType = kWMDRMType;
    OMX_U32 ret = AMP_CBUFCreate(frame_in_buf_size, 0, frame_in_bd_num,
        frame_in_align_size, true, &mPool, true, frame_in_pad_size, AMP_SHM_SECURE_STATIC, false);
    if (AMPCBUF_SUCCESS != ret) {
      OMX_LOGE("Secure Cbuf Create Error %u.", ret);
      return OMX_ErrorUndefined;
    }
    ret = AMP_CBUFGetShm(mPool, &mShmHandle);
    if (AMPCBUF_SUCCESS != ret) {
      OMX_LOGE("Secure Cbuf Get Shm Handle Error %u.", ret);
      return OMX_ErrorUndefined;
    }
    config._u.pVDEC.uiFlag = kFrameInModeMask;
    config._u.pVDEC.uiSHMSize = frame_in_buf_size;
    OMX_PARAM_PORTDEFINITIONTYPE def;
    mInputPort->getDefinition(&def);
    def.nBufferCountActual = 4;
    mInputPort->setDefinition(&def);
  } else if (0 == kdMemcmp(mDrm.nUUID, kUuidMarLin, sizeof(mDrm.nUUID))) {
    OMX_LOGD("Secure Marlin Tvp");
    configTvpVersion();
    // marlin only supportted by tvp2.0
    if (mInputPort->mAmpDisableTvp == OMX_TRUE) {
      mInputPort->configDrm(kMarLinType, &es_handle);
      config._u.pVDEC.uiFlag = kStreamInModeMask;
    } else {
      OMX_LOGW("Marlin only supportted by tvp2.0.");
      return OMX_ErrorUndefined;
    }
  } else if (0 != kdMemcmp(mDrm.nUUID, kUuidNoDrm, sizeof(mDrm.nUUID))) {
    OMX_LOGD("Unsupported DRM: "
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        mDrm.nUUID[0], mDrm.nUUID[1], mDrm.nUUID[2], mDrm.nUUID[3],
        mDrm.nUUID[4], mDrm.nUUID[5], mDrm.nUUID[6], mDrm.nUUID[7],
        mDrm.nUUID[8], mDrm.nUUID[9], mDrm.nUUID[10], mDrm.nUUID[11],
        mDrm.nUUID[12], mDrm.nUUID[13], mDrm.nUUID[14], mDrm.nUUID[15]);
      return OMX_ErrorUndefined;
  } else {
    if (mOutputPort->isTunneled()) {
#ifdef VDEC_USE_FRAME_IN_MODE
      config._u.pVDEC.uiFlag = kFrameInModeMask;
      OMX_PARAM_PORTDEFINITIONTYPE def;
      mInputPort->getDefinition(&def);
      def.nBufferCountActual = 32;
      mInputPort->setDefinition(&def);
#else
      OMX_U32 ret;
#ifdef VDEC_USE_CBUF_STREAM_IN_MODE
      if (amp_codec == MEDIA_VES_VP8) {
        // enlarge vp8 threshold as 64k to adapt AMP vp8 decoding.
        ret = AMP_CBUFCreate(stream_in_buf_size, 0, stream_in_bd_num,
            stream_in_align_size * 16, true, &mPool, false, 0, AMP_SHM_STATIC, true);
      } else {
        ret = AMP_CBUFCreate(stream_in_buf_size, 0, stream_in_bd_num,
            stream_in_align_size, true, &mPool, false, 0, AMP_SHM_STATIC, true);
      }
      config._u.pVDEC.uiFlag = kStreamInModeMask;
      config._u.pVDEC.uiSHMSize = stream_in_buf_size;
#else
      ret = AMP_CBUFCreate(frame_in_buf_size, 0, frame_in_bd_num,
          frame_in_align_size, true, &mPool, true, frame_in_pad_size, AMP_SHM_STATIC, true);
      config._u.pVDEC.uiFlag = kFrameInModeMask;
      config._u.pVDEC.uiSHMSize = frame_in_buf_size;
#endif
      if (AMPCBUF_SUCCESS != ret) {
        OMX_LOGE("Cbuf Create Error %u.", ret);
        return OMX_ErrorUndefined;
      }
      OMX_PARAM_PORTDEFINITIONTYPE def;
      mInputPort->getDefinition(&def);
      def.nBufferCountActual = 4;
      mInputPort->setDefinition(&def);
#endif
    } else {
      if (mSecure == OMX_TRUE) {
        OMX_LOGD("Enable Secure No Tvp Playback.");
        mInputPort->mOmxDisableTvp = OMX_TRUE;
        TEEC_Result result = TEEC_InitializeContext(NULL, &mContext);
        if (result != TEEC_SUCCESS) {
          OMX_LOGE("Fail to initialize the TEE Context, err %d.", result);
          return OMX_ErrorUndefined;
        }
      }
      config._u.pVDEC.uiFlag = kFrameInModeMask;
      OMX_PARAM_PORTDEFINITIONTYPE def;
      mInputPort->getDefinition(&def);
      def.nBufferCountActual = 16;
      mInputPort->setDefinition(&def);
      if (def.format.video.eColorFormat ==
          static_cast<OMX_COLOR_FORMATTYPE>(HAL_PIXEL_FORMAT_YV12)) {
        config._u.pVDEC.uiFlag |= kYV12OutputMask;
      }
    }
  }

  if (!mInputPort->mOmxDisableTvp) {
    if (amp_codec == MEDIA_VES_AVC) {
      mExtraData = new AvcExtraData();
    } else {
      mExtraData = new CodecExtraData();
    }
  } else {
    mSecureExtraData = new SecureCodecExtraData(&mContext);
    mInputPort->InitalSecurePadding(&mContext);
  }

  // consider other player may not set the container type,
  // we just display the copy frames for the Mp4 streams
  // to pass the gtvv4 tv-cts.
  if (mContainerType.eFmtType == OMX_FORMAT_MP4) {
    config._u.pVDEC.uiFlag |= kDisplayCopyframeMask;
    OMX_LOGD("Display the copy frames.");
  }
#ifdef _ANDROID_
  // Add low latency mode for ubitus
  char prop_value[PROPERTY_VALUE_MAX];
  if (property_get("service.media.low_latency", prop_value, NULL) > 0) {
    OMX_LOGD("Get low latency value as %s.", prop_value);
    if (strcasecmp("true", prop_value) == 0) {
      config._u.pVDEC.uiFlag |= kLowLatencyModeMask;
      OMX_LOGD("Set video low latency mode on OMX.");
    }
  } else {
      OMX_LOGW("Failed to get lowlatency mode.");
  }
#endif
  AMP_RPC(err, AMP_VDEC_Open, mAmpHandle, &config);
  CHECKAMPERRLOG(err, "open VDEC handler");

  if (mInputPort->mIsDrm && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = getAmpDmxComponent();
    CHECKAMPERRLOG(err, "get AMP Dmx Component.");

    AMP_COMPONENT_CONFIG dmx_config;
    kdMemset(&dmx_config, 0 , sizeof(AMP_COMPONENT_CONFIG));
    dmx_config._d = AMP_COMPONENT_DMX;
    dmx_config._u.pDMX.mode = AMP_SECURE_TUNNEL;
    dmx_config._u.pDMX.uiInputPortNum = 1;
    dmx_config._u.pDMX.uiOutputPortNum = 128;
    AMP_RPC(err, AMP_DMX_Open, mDMXHandle, &dmx_config);
    CHECKAMPERRLOG(err, "open DMX handler");

    AMP_DMX_INPUT_CFG input_config;
    kdMemset(&input_config, 0, sizeof(AMP_DMX_INPUT_CFG));
    input_config.InputType = AMP_DMX_INPUT_ES;
    input_config.hInBuf = es_handle;
    input_config.uiBufSize = es_buf_size;
    input_config.uiBufPaddingSize = es_buf_pad_size;
    if (mInputPort->mDrmType == kPlayReadyType
        || mInputPort->mDrmType == kMarLinType) {
      input_config.CyptCfg.SchemeType = AMP_DMX_AES_128_CTR_NO_NO;
    } else if (mInputPort->mDrmType == kWideVineType) {
      input_config.CyptCfg.SchemeType = AMP_DMX_AES_128_CBC_CTS_CLR;
    }
    AMP_RPC(err, AMP_DMX_AddInput, mDMXHandle, &input_config, &mInputObj);
    CHECKAMPERRLOG(err, "add input to DMX handler");
    OMX_LOGD("DMX input port index %d", mInputObj.uiPortIdx);

    AMP_DMX_CH_CFG channel_config;
    kdMemset(&channel_config, 0 , sizeof(AMP_DMX_CH_CFG));
    channel_config.eType = AMP_DMX_OUTPUT_VES;
    channel_config.Tag = 0;
    channel_config.uAlign = 2;
    channel_config.SchemeType = AMP_DMX_CRYPTO_SCHEME_TYPE_NONE;
    mChnlObj.uiPortIdx = AMP_DMX_ANY_PORT_IDX;
    AMP_RPC(err, AMP_DMX_AddChannel, mDMXHandle, &channel_config, &mChnlObj);
    CHECKAMPERRLOG(err, "add channel to DMX handler");
    OMX_LOGD("DMX output port index %d", mChnlObj.uiPortIdx);
  }

  AMP_PORT_INFO input_port_info, output_port_info;

  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = AMP_ConnectApp(mDMXHandle, AMP_PORT_INPUT,
        mInputObj.uiPortIdx, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect VideoDecoder input port with DMX");
    err = AMP_ConnectComp(mDMXHandle, mChnlObj.uiPortIdx,
        mAmpHandle, 0);
    CHECKAMPERRLOG(err, "connect DMX with VDEC");
  } else {
    AMP_RPC(err, AMP_VDEC_QueryPort, mAmpHandle,
        AMP_PORT_INPUT, 0, &input_port_info);
    CHECKAMPERRLOG(err, "query info of VDEC input port");
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_INPUT,
        0, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect video decoder with VDEC input port");
  }

  if (!mOutputPort->isTunneled()) {
    AMP_RPC(err, AMP_VDEC_QueryPort, mAmpHandle, AMP_PORT_OUTPUT,
        0, &output_port_info);
    CHECKAMPERRLOG(err, "query info of VDEC output port");
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_OUTPUT,
        0, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect video decoder with VDEC output port");
  }

  // output port 1 is for user data CC/AFD
  err = AMP_ConnectApp(mAmpHandle, AMP_PORT_OUTPUT, 1, vdecUserDataCb, static_cast<void *>(this));
  CHECKAMPERRLOG(err, "connect vdecUserDataCb");

  // Set the frame rate to vdec for calculating the pts of the output frames if the input frame
  // pts is invalid value.
  OMX_LOGD("xFramerate is %u", mInputPort->getVideoDefinition().xFramerate);
  if (mInputPort->getVideoDefinition().xFramerate > 0 &&
      mInputPort->getVideoDefinition().xFramerate <= kMaxFrameRate << 16) {
    AMP_RPC(err, AMP_VDEC_SetCmdP2, mAmpHandle, AMP_VDEC_CMD_SET_FRAME_RATE,
        mInputPort->getVideoDefinition().xFramerate, 0x10000);
    CHECKAMPERRLOG(err, "set video frame rate.");
  } else {
    OMX_LOGW("The xFramerate is abnormal, OMX can't send it to VDEC."
        "VDEC will parse the Framerate from ES data.");
  }

  //disable AFD user data in non-tunnel mode to avoid regression
  if (mOutputPort->isTunneled()) {
    AMP_RPC(err, AMP_VDEC_SetCmdP2, mAmpHandle, AMP_VDEC_CMD_SET_USER_DATA,
      VIDEO_USER_DATA_TYPE_AFD,  AMP_VDEC_IOPARAM_USER_DATA_ENABLE);
  }

  // Enable fp user data VIDEO_USER_DATA_TYPE_FP.
  // Now, the non-tunnel mode has not supported 3D, we do not enable it first.
  if (mOutputPort->isTunneled()) {
    AMP_RPC(err, AMP_VDEC_SetCmdP2, mAmpHandle, AMP_VDEC_CMD_SET_USER_DATA,
          VIDEO_USER_DATA_TYPE_FP,  AMP_VDEC_IOPARAM_USER_DATA_ENABLE);
  }

  // Register a callback so that it can receive event - for example resolution changed -from VDEC.
  mListener = AMP_Event_CreateListener(AMP_EVENT_TYPE_MAX, 0);
  if (mListener) {
    err = registerEvent(AMP_EVENT_API_VDEC_CALLBACK);
    CHECKAMPERRLOG(err, "RegisterEvent with VDEC");
    err = registerEvent(AMP_VDEC_EVENT_3D_MODE_CHANGE);
    CHECKAMPERRLOG(err, "register 3D_MODE_CHANGE Event with VDEC");
    err = registerEvent(AMP_EVENT_API_VIDEO_UNDERFLOW);
    CHECKAMPERRLOG(err, "register AMP_EVENT_API_VIDEO_UNDERFLOW Event with VDEC");
  }

  memset(&mVdecInfo, 0 , sizeof(mVdecInfo));
  mVdecInfo.nStereoMode = marvell::kStereoNone;
  mSourceControl = new SourceControl();
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::release() {
  OMX_LOG_FUNCTION_ENTER;
  // deinit mediahelper
  if (mMediaHelper == OMX_TRUE) {
    mediainfo_teardown();
  }
  if (!mInputPort->mOmxDisableTvp) {
    if (mExtraData) {
      delete mExtraData;
      mExtraData = NULL;
    }
  } else {
    if (mSecureExtraData) {
      delete mSecureExtraData;
      mSecureExtraData = NULL;
    }
    mInputPort->FinalizeSecurePadding();
    TEEC_FinalizeContext(&mContext);
  }
  if (mPool) {
    if (AMPCBUF_SUCCESS != AMP_CBUFDestroy(mPool)) {
      OMX_LOGE("Cbuf destroy pool error.");
    }
  }

#ifdef _ANDROID_
  char prop_value[PROPERTY_VALUE_MAX];
  if (property_get("service.media.low_latency", prop_value, NULL) > 0) {
    if (strcasecmp("true", prop_value) == 0) {
      // disable lowlatency for ubitus
      OMX_LOGD("Disable low latency mode on OMX.");
      if (property_set("service.media.low_latency", "false") !=0 ) {
        OMX_LOGE("Dsiable low ltaency failed.");
      }
    }
  }
#endif
  HRESULT err = SUCCESS;
  if (mListener) {
    err = unRegisterEvent(AMP_EVENT_API_VDEC_CALLBACK);
    CHECKAMPERRLOG(err, "unRegisterEvent with VDEC");
    err = unRegisterEvent(AMP_VDEC_EVENT_3D_MODE_CHANGE);
    CHECKAMPERRLOG(err, "unRegister 3D ModeChange Event with VDEC");
    err = unRegisterEvent(AMP_EVENT_API_VIDEO_UNDERFLOW);
    CHECKAMPERRLOG(err, "unRegister AMP_EVENT_API_VIDEO_UNDERFLOW Event with VDEC");
    CHECKAMPERR(AMP_Event_DestroyListener(mListener));
    mListener = NULL;
  }

  if (mInputPort->mIsDrm && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = AMP_DisconnectApp(mDMXHandle, AMP_PORT_INPUT,
        mInputObj.uiPortIdx, pushBufferDone);
    CHECKAMPERRLOG(err, "disconnect video decoder with DMX input port");
    err = AMP_DisconnectComp(mDMXHandle, mChnlObj.uiPortIdx,
        mAmpHandle, 0);
    CHECKAMPERRLOG(err, "disconnect DMX with VDEC");
  } else {
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_INPUT,
        0, pushBufferDone);
    CHECKAMPERRLOG(err, "disconnect video decoder with VDEC input port");
  }

  // output port 1 is for user data CC/AFD
  err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_OUTPUT, 1, vdecUserDataCb);
  CHECKAMPERRLOG(err, "disconnect vdecUserDataCb");

  if (!mOutputPort->isTunneled()) {
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_OUTPUT,
        0, pushBufferDone);
    CHECKAMPERRLOG(err, "disconnect video decoder with VDEC output port");
  } else {
    OMX_HANDLETYPE omx_handle = mOutputPort->getTunnelComponent();
    if (NULL != omx_handle) {
      OmxComponentImpl *omx_vout = static_cast<OmxComponentImpl *>(reinterpret_cast<
          OMX_COMPONENTTYPE *>(omx_handle)->pComponentPrivate);
      if (NULL != omx_vout) {
        AMP_COMPONENT amp_vout = omx_vout->getAmpHandle();
        if (NULL != amp_vout) {
          //To ensure the Amp Vout component is in Idle state when disconnect it with Vdec.
          AMP_RPC(err, AMP_VOUT_SetState, amp_vout, AMP_IDLE);
          CHECKAMPERRLOG(err, "set vout to idle state.");
          err = AMP_DisconnectComp(mAmpHandle, 0, amp_vout, 0);
          CHECKAMPERRLOG(err, "disconnect video decoder with VOUT input port");
          OMX_LOGD("disconnect vdec and vout in vdec component.");
        } else {
          OMX_LOGD("amp vout is already released.");
        }
      } else {
        OMX_LOGD("omx vout component is already released.");
      }
    } else {
      OMX_LOGD("omx vout handle is already released.");
    }
  }

  if (mInputPort->mIsDrm && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    AMP_RPC(err, AMP_DMX_RmChannel, mDMXHandle, &mChnlObj);
    CHECKAMPERRLOG(err, "Rm DMX channel");
    AMP_RPC(err, AMP_DMX_RmInput, mDMXHandle, &mInputObj);
    CHECKAMPERRLOG(err, "Rm DMX input");
    AMP_RPC(err,AMP_DMX_Close, mDMXHandle);
    CHECKAMPERRLOG(err, "close DMX component");
  }

  AMP_RPC(err, AMP_VDEC_Close, mAmpHandle);
  CHECKAMPERRLOG(err, "close VDEC");

  if (mInputPort->mIsDrm && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = destroyAmpDmxComponent();
    CHECKAMPERRLOG(err, "destroy DMX component");
  }

  err = destroyAmpVideoDecoder();
  CHECKAMPERRLOG(err, "destroy VDEC");

  if (OMX_TRUE == mInputPort->mIsDrm) {
    OMX_LOGD("releaseDrm");
    mInputPort->releaseDrm();
  }

  if ((mSourceControl != NULL) && (-1 != mSourceId)) {
    mSourceControl->removeObserver(mObserver);
    mSourceControl->exitSource(mSourceId);
    destroyAmpDisplayService();
  }

  if (mTsRecover) {
    delete mTsRecover;
    mTsRecover = NULL;
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::preroll() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::pause() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mPaused = OMX_TRUE;
  mShouldExit = OMX_TRUE;
  err = setAmpState(AMP_PAUSED);
  CHECKOMXERR(err);
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::resume() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = setDmxChannel(OMX_TRUE);
    CHECKAMPERRLOG(err, "turn on DMX channel");
  }
  err = setAmpState(AMP_EXECUTING);
  CHECKOMXERR(err);
  if (mInputPort->getCachedBuffer() != NULL) {
    returnBuffer(mInputPort, mInputPort->getCachedBuffer());
  }
  if (mOutputPort->getCachedBuffer() != NULL) {
    returnBuffer(mOutputPort, mOutputPort->getCachedBuffer());
  }
  mShouldExit = OMX_FALSE;
  kdThreadMutexLock(mPauseLock);
  mPaused = OMX_FALSE;
  kdThreadCondSignal(mPauseCond);
  kdThreadMutexUnlock(mPauseLock);
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::start() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  err = registerBds(mInputPort);
  CHECKOMXERR(err);
  err = registerBds(mOutputPort);
  CHECKOMXERR(err);
  if (mOutputPort->isTunneled() == OMX_FALSE) {
    mHasRegisterOutBuf = OMX_TRUE;
  }
  OMX_LOGD("Create decoding thread");
  mPauseLock = kdThreadMutexCreate(KD_NULL);
  mPauseCond = kdThreadCondCreate(KD_NULL);
  mBufferLock = kdThreadMutexCreate(KD_NULL);
  mThread = kdThreadCreate(mThreadAttr, threadEntry,(void *)this);
  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = setDmxChannel(OMX_TRUE);
    CHECKAMPERRLOG(err, "turn on DMX channel");
  }
  err = setAmpState(AMP_EXECUTING);
  CHECKOMXERR(err);
  bindPQSource();
  if (-1 != mSourceId) {
    mObserver = new AVSettingObserver(this);
    status_t ret = mSourceControl ->registerObserver("mrvl.source", mObserver);
    if (android::OK == ret) {
      OMX_LOGD("Rigister AVSettingObserver success.");
      getAmpDisplayService();
    } else {
      OMX_LOGE("Rigister AVSettingObserver failed.");
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::stop() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mShouldExit = OMX_TRUE;
  if (mPauseLock) {
    kdThreadMutexLock(mPauseLock);
    mPaused = OMX_FALSE;
    kdThreadCondSignal(mPauseCond);
    kdThreadMutexUnlock(mPauseLock);
  }
  if (mThread) {
    // post buffer may cause the sem value not correct
    // TODO: need correct the value
    if (mInputPort->isEmpty())
      mInputPort->postBuffer();
    if (mOutputPort->isEmpty())
      mOutputPort->postBuffer();
    void *retval;
    KDint ret;
    ret = kdThreadJoin(mThread, &retval);
    OMX_LOGD("Stop decoding thread");
    mThread = NULL;
  }
  if (mPauseLock) {
    kdThreadMutexFree(mPauseLock);
    mPauseLock = NULL;
  }
  if (mPauseCond) {
    kdThreadCondFree(mPauseCond);
    mPauseCond = NULL;
  }
  if (mBufferLock) {
    kdThreadMutexFree(mBufferLock);
    mBufferLock = NULL;
  }
  if (NULL != mCachedhead) {
    OMX_LOGD("return the cached buffer header.");
    returnBuffer(mInputPort, mCachedhead);
    mCachedhead = NULL;
  }
  if (NULL != mCryptoInfo) {
    OMX_LOGD("release the cached cryptoInfo.");
    kdFree(mCryptoInfo);
    mCryptoInfo = NULL;
  }
  err = setAmpState(AMP_IDLE);
  CHECKOMXERR(err);
  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = clearAmpDmxPort();
    CHECKAMPERRLOG(err, "clear DMX port");
    err = setDmxChannel(OMX_FALSE);
    CHECKAMPERRLOG(err, "turn off DMX channel");
  }
  // TODO: Change to wait event when AMP side is ready
  // Waiting for input/output buffers back
  OMX_U32 wait_count = 100;
  while ((mInputFrameNum > mInBDBackNum || mOutBDPushNum > mOutputFrameNum)
      && wait_count > 0) {
    usleep(5000);
    wait_count--;
  }
  if (!wait_count) {
    OMX_LOGE("There are %u inbuf and %u outbuf have not returned.",
        mInputFrameNum - mInBDBackNum, mOutBDPushNum - mOutputFrameNum);
  }
  if (NULL != mPool) {
    wait_count = 100;
    while (mPushedBdNum > mReturnedBdNum && wait_count > 0) {
      usleep(5000);
      wait_count--;
    }
    if (!wait_count) {
      OMX_LOGE("There are %u pushed Bd not returned.", mPushedBdNum - mReturnedBdNum);
    }
  }
  returnCachedBuffers(mInputPort);
  returnCachedBuffers(mOutputPort);
  err = unregisterBds(mInputPort);
  CHECKOMXERR(err);
  err = unregisterBds(mOutputPort);
  CHECKOMXERR(err);
  mHasRegisterOutBuf = OMX_FALSE;
  mStartPushOutBuf = OMX_FALSE;
  mShouldExit = OMX_FALSE;
  mInited = OMX_FALSE;
  mStreamPosition = 0;
  mEOS = OMX_FALSE;
  mOutputEOS = OMX_FALSE;
  if (mTsRecover) {
    mTsRecover->clear();
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::flush() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 wait_count = 100;
  if (!mOutputPort->isTunneled()) {
    // As it would call flushPort() later and operate port->isEmpty(),
    // And pushBufferLoop() will operate port->isEmpty() in another thread,
    // so make sure port->isEmpty() true here to avoid return buffer two times
    OMX_BUFFERHEADERTYPE *in_head = NULL, *out_head = NULL;
    while(!mInputPort->isEmpty()) {
      in_head = mInputPort->popBuffer();
      if (in_head != NULL) {
        OMX_LOGD("return in_head %p when flushing", in_head);
        returnBuffer(mInputPort, in_head);
        in_head = NULL;
      }
    }
    while(!mOutputPort->isEmpty()) {
      out_head = mOutputPort->popBuffer();
      if (out_head != NULL) {
        OMX_LOGD("return out_head %p when flushing", out_head);
        returnBuffer(mOutputPort, out_head);
        out_head = NULL;
      }
    }
  }
  kdThreadMutexLock(mBufferLock);
  if (NULL != mCachedhead) {
    OMX_LOGD("return the cached buffer header.");
    returnBuffer(mInputPort, mCachedhead);
    mCachedhead = NULL;
  }
  if (NULL != mCryptoInfo) {
    OMX_LOGD("release the cached cryptoInfo.");
    kdFree(mCryptoInfo);
    mCryptoInfo = NULL;
  }
  err = setAmpState(AMP_IDLE);
  CHECKOMXERRNOTRETURN(err);
  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    err = clearAmpDmxPort();
    CHECKAMPERRLOGNOTRETURN(err, "clear DMX port");
    err = setDmxChannel(OMX_FALSE);
    CHECKAMPERRLOGNOTRETURN(err, "turn off DMX channel");
  }
  // clear dumped data
  if (CheckMediaHelperEnable(mMediaHelper, CLEAR_DUMP) == OMX_TRUE) {
    mediainfo_remove_dumped_data(MEDIA_VIDEO);
  }
  // TODO: Waiting for input/output buffer back
  wait_count = 100;
  while ((mInputFrameNum > mInBDBackNum || mOutBDPushNum > mOutputFrameNum)
      && wait_count > 0) {
    usleep(5000);
    wait_count--;
  }
  if (!wait_count) {
    OMX_LOGE("There are %u inbuf and %u outbuf have not returned.",
        mInputFrameNum - mInBDBackNum, mOutBDPushNum - mOutputFrameNum);
  }
  if (NULL != mPool) {
    wait_count = 100;
    while (mPushedBdNum > mReturnedBdNum && wait_count > 0) {
      usleep(5000);
      wait_count--;
    }
    if (!wait_count) {
      OMX_LOGE("There are %u pushed Bd not returned.", mPushedBdNum - mReturnedBdNum);
    }
    if (AMPCBUF_SUCCESS != AMP_CBUFReset(mPool)) {
      OMX_LOGE("Cbuf AMP_CBUFReset error.");
    }
    mPushedBdNum = 0;
    mReturnedBdNum = 0;
    mAddedPtsTagNum = 0;
  }
  returnCachedBuffers(mInputPort);
  returnCachedBuffers(mOutputPort);
  if (OMX_TRUE == mInputPort->mIsDrm) {
    if (mInputPort->mAmpDisableTvp == OMX_TRUE) {
      mDataCount = 0;
    }
    mInputPort->clearMemCtrlIndex();
  }
  if (getState() == OMX_StateExecuting) {
    err = setAmpState(AMP_EXECUTING);
  } else if (getState() == OMX_StatePause) {
    err = setAmpState(AMP_PAUSED);
  }
  CHECKOMXERRNOTRETURN(err);
  mStreamPosition = 0;
  mInited = OMX_FALSE;
  mEOS = OMX_FALSE;
  mOutputEOS = OMX_FALSE;
  mInputFrameNum = 0;
  mOutputFrameNum = 0;
  mInBDBackNum = 0;
  mOutBDPushNum = 0;
  kdThreadMutexUnlock(mBufferLock);
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

#ifdef VIDEO_DECODE_ONE_FRAME_MODE
void OmxAmpVideoDecoder::onPortVideoDecodeMode(
    OMX_U32 enable, OMX_S32 mode) {
  if (mAmpHandle) {
    HRESULT err = SUCCESS;
    if (enable == 1) {
        OMX_LOGI("Set video decoder mode as 1-frame auto stop mode");
        AMP_RPC(err, AMP_VDEC_SetCmdP1, mAmpHandle, AMP_VDEC_CMD_SET_DECODE,
            AMP_VDEC_IOPARAM_DECODE_1_AUTO_STOP);
        if (SUCCESS != err) {
            OMX_LOGE("Failed to enable mode AMP_VDEC_IOPARAM_DECODE_1_AUTO_STOP");
        }
    } else {
        AMP_RPC(err, AMP_VDEC_SetCmdP1, mAmpHandle, AMP_VDEC_CMD_SET_DECODE,
            AMP_VDEC_IOPARAM_DECODE_ALL);
        err = setAmpState(AMP_EXECUTING);
        CHECKOMXERRNOTRETURN(err);
        OMX_LOGI("Set video decoder mode as deocde all frames mode");
        if (SUCCESS != err) {
            OMX_LOGE("Failed to enable mode AMP_VDEC_IOPARAM_DECODE_ALL");
        }
    }

  }
}
#endif

void OmxAmpVideoDecoder::onPortEnableCompleted(OMX_U32 portIndex, OMX_BOOL enabled) {
  if (portIndex == (kVideoPortStartNumber + 1)) {
    OMX_LOGD("onPortEnableCompleted %d -> %d", mStartPushOutBuf, enabled);
    mStartPushOutBuf = enabled;
  }
}

OMX_ERRORTYPE OmxAmpVideoDecoder::getAmpVideoDecoder() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  AMP_FACTORY factory;
  err = AMP_GetFactory(&factory);
  CHECKAMPERRLOG(err, "get AMP factory");

  AMP_RPC(err, AMP_FACTORY_CreateComponent, factory, AMP_COMPONENT_VDEC,
      1, &mAmpHandle);
  CHECKAMPERRLOG(err, "create VDEC component");

  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::destroyAmpVideoDecoder() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mAmpHandle) {
    AMP_RPC(err, AMP_VDEC_Destroy, mAmpHandle);
    CHECKAMPERRLOG(err, "destory VDEC");

    AMP_FACTORY_Release(mAmpHandle);
    mAmpHandle = NULL;
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::getAmpDisplayService() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  AMP_FACTORY factory;
  err = AMP_GetFactory(&factory);
  CHECKAMPERRLOG(err, "get AMP factory");

  AMP_RPC(err, AMP_FACTORY_CreateDisplayService, factory, &mDispHandle);
  CHECKAMPERRLOG(err, "create Display Service");

  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::destroyAmpDisplayService() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mDispHandle) {
    AMP_RPC(err, AMP_DISP_Destroy, mDispHandle);
    CHECKAMPERRLOG(err, "destory Disp");

    AMP_FACTORY_Release(mDispHandle);
    mDispHandle = NULL;
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::getAmpDmxComponent() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  AMP_FACTORY factory;
  err = AMP_GetFactory(&factory);
  CHECKAMPERRLOG(err, "get AMP factory");

  AMP_RPC(err, AMP_FACTORY_CreateComponent, factory, AMP_COMPONENT_DMX,
      0, &mDMXHandle);
  CHECKAMPERRLOG(err, "create DMX component");

  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::destroyAmpDmxComponent() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mDMXHandle) {
    AMP_RPC(err, AMP_DMX_Destroy, mDMXHandle);
    CHECKAMPERRLOG(err, "destory DMX component");

    AMP_FACTORY_Release(mDMXHandle);
    mDMXHandle = NULL;
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::clearAmpDmxPort() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_DMX_ClearPortBuf, mDMXHandle, AMP_PORT_INPUT,
      mInputObj.uiPortIdx);
  CHECKAMPERRLOG(err, "clear DMX input port buffer");

  AMP_RPC(err, AMP_DMX_ClearPortBuf, mDMXHandle, AMP_PORT_OUTPUT,
      mChnlObj.uiPortIdx);
  CHECKAMPERRLOG(err, "clear DMX output port buffer");
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::setDmxChannel(OMX_BOOL on) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (on == OMX_TRUE) {
    AMP_RPC(err, AMP_DMX_ChannelControl, mDMXHandle, &mChnlObj,
        AMP_DMX_CH_CTRL_ON, 0);
    CHECKAMPERRLOG(err, "turn on DMX channel");
  } else {
    AMP_RPC(err, AMP_DMX_ChannelControl, mDMXHandle, &mChnlObj,
        AMP_DMX_CH_CTRL_OFF, 0);
    CHECKAMPERRLOG(err, "turn off DMX channel");
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::configTvpVersion() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  // Parse config file to decide which Tvp need to be selected.
  BerlinOmxParseXmlCfg *parse = new BerlinOmxParseXmlCfg("/etc/berlin_config_sw.xml");
  if (parse->parseConfig("DMX", "TVPVersion") == OMX_TRUE) {
    char *data = parse->getParseData();
    if (data != NULL) {
      if (kdMemcmp(data, "1", 1) == 0) {
        OMX_LOGD("Use Tvp1.0.");
        mInputPort->mAmpDisableTvp = OMX_FALSE;
      } else {
        OMX_LOGD("Use Tvp2.0.");
        mInputPort->mAmpDisableTvp = OMX_TRUE;
      }
    } else {
      OMX_LOGD("Fail to get the parse data, use Tvp1.0.");
      mInputPort->mAmpDisableTvp = OMX_FALSE;
    }
  } else {
    OMX_LOGD("Fail to parse the config file, use Tvp1.0.");
    mInputPort->mAmpDisableTvp = OMX_FALSE;
  }
  delete parse;
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_BOOL OmxAmpVideoDecoder::isVideoParamSupport(
    OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param) {
  // TODO: implement this
  OMX_BOOL isSupport = OMX_FALSE;
  switch (mInputPort->getVideoDefinition().eCompressionFormat) {
    case OMX_VIDEO_CodingAVC:
    case OMX_VIDEO_CodingMPEG4:
    case OMX_VIDEO_CodingMPEG2:
    case OMX_VIDEO_CodingH263:
#ifdef _ANDROID_
    case OMX_VIDEO_CodingVPX:
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

OMX_BOOL OmxAmpVideoDecoder::isProfileLevelSupport(
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *prof_lvl) {
  // TODO: implement this
  OMX_BOOL isSupport = OMX_FALSE;
  switch(mInputPort->getVideoDefinition().eCompressionFormat) {
    case OMX_VIDEO_CodingAVC:
    case OMX_VIDEO_CodingMPEG4:
    case OMX_VIDEO_CodingMPEG2:
    case OMX_VIDEO_CodingH263:
#ifdef _ANDROID_
    case OMX_VIDEO_CodingVPX:
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

OMX_ERRORTYPE OmxAmpVideoDecoder::setAmpState(AMP_STATE state) {
  OMX_LOGD("setAmpState %s", AmpState2String(state));
  HRESULT err = SUCCESS;

  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE) {
    OMX_LOGD("start set DMX state.");
    AMP_RPC(err, AMP_DMX_SetState, mDMXHandle, state);
    CHECKAMPERRLOG(err, "set DMX state");
    OMX_LOGD("set DMX state done.");
  }

  OMX_LOGD("start set VDEC state.");
  AMP_RPC(err, AMP_VDEC_SetState, mAmpHandle, state);
  CHECKAMPERRLOG(err, "set VDEC state");
  OMX_LOGD("set VDEC state done.");

  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::getAmpState(AMP_STATE *state) {
  HRESULT err = SUCCESS;

  AMP_RPC(err, AMP_VDEC_GetState, mAmpHandle, state);
  CHECKAMPERRLOG(err, "get VDEC state");

  OMX_LOGD("getAmpState is %s", AmpState2String(*state));
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::registerBds(OmxAmpVideoPort *port) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  UINT32 uiPortIdx = 0;
  if (port->isInput()) {
    for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
      AmpBuffer *amp_buffer = port->getAmpBuffer(i);
      if (NULL != amp_buffer) {
        if (OMX_FALSE == port->mIsDrm) {
          amp_buffer->addMemInfoTag();
          amp_buffer->addPtsTag();
          AMP_RPC(err, AMP_VDEC_RegisterBD, mAmpHandle, AMP_PORT_INPUT,
              uiPortIdx, amp_buffer->getBD());
          CHECKAMPERRLOG(err, "register BDs at VDEC input port");
        }
      }
    }
  } else {
    for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
      AmpBuffer *amp_buffer = port->getAmpBuffer(i);
      if (NULL != amp_buffer) {
        amp_buffer->addMemInfoTag();
        amp_buffer->addFrameInfoTag();
        AMP_RPC(err, AMP_VDEC_RegisterBD, mAmpHandle, AMP_PORT_OUTPUT,
            uiPortIdx, amp_buffer->getBD());
        CHECKAMPERRLOG(err, "regsiter BDs at VDEC output port");
        OMX_LOGD("AMP_VDEC_RegisterBD, portIo 1, bd %d", amp_buffer->getBD());
      }
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::unregisterBds(OmxAmpVideoPort *port) {
  OMX_LOG_FUNCTION_ENTER;
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
    if (NULL != bd && OMX_FALSE == port->mIsDrm) {
      AMP_RPC(err, AMP_VDEC_UnregisterBD, mAmpHandle, portIo, uiPortIdx, bd);
      CHECKAMPERRLOG(err, "unregsiter BDs for VDEC");

      OMX_LOGD("AMP_VDEC_UnregisterBD, portIo %d, bd %p", portIo, bd);
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::pushAmpBd(AMP_PORT_IO port,
    UINT32 portindex, AMP_BD_ST *bd) {
  if (CheckMediaHelperEnable(mMediaHelper, PUSH_AMPBD) == OMX_TRUE) {
    OMX_LOGD("(%s)pushAmpBd, bd 0x%x, bdid 0x%x, allocva 0x%x",
        port == AMP_PORT_INPUT ? "In":"Out", bd, bd->uiBDId, bd->uiAllocVA);
  }
  HRESULT err = SUCCESS;
  if (mInputPort->mIsDrm == OMX_TRUE && mInputPort->mAmpDisableTvp == OMX_TRUE
      && port == AMP_PORT_INPUT) {
    AMP_RPC(err, AMP_VDEC_PushBD, mDMXHandle, port, portindex, bd);
    CHECKAMPERRLOG(err, "push BD to DMX");
  } else {
    AMP_RPC(err, AMP_VDEC_PushBD, mAmpHandle, port, portindex, bd);
    CHECKAMPERRLOG(err, "push BD to VDEC");
  }

  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::resetPtsForFirstFrame(
    OMX_BUFFERHEADERTYPE *in_head) {
// TODO: Need to set a true value If the first frame pts is invalid after seeking
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OmxPortImpl *port0 = getPort(kVideoPortStartNumber + 1);
  if (port0 && port0->isTunneled()) {
    OMX_COMPONENTTYPE *video_processor =
        static_cast<OMX_COMPONENTTYPE *>(port0->getTunnelComponent());
    if (video_processor && video_processor->pComponentPrivate) {
      OmxPortImpl *port1 = static_cast<OmxComponentImpl *>(
          video_processor->pComponentPrivate)->getPort(kVideoPortStartNumber + 1);
      if (port1 && port1->isTunneled()) {
        OMX_COMPONENTTYPE *video_scheduler =
            static_cast<OMX_COMPONENTTYPE *>(port1->getTunnelComponent());
        if (video_scheduler && video_scheduler->pComponentPrivate) {
          OmxPortImpl *port2 = static_cast<OmxComponentImpl *>(
              video_scheduler->pComponentPrivate)->getPort(kClockPortStartNumber);
          if (port2 && port2->isTunneled()) {
            OMX_COMPONENTTYPE *clock =
                static_cast<OMX_COMPONENTTYPE *>(port2->getTunnelComponent());
            OMX_TIME_CONFIG_CLOCKSTATETYPE state;
            OMX_U32 wait_count = 20;
            InitOmxHeader(&state);
            while (wait_count > 0) {
              err = OMX_GetConfig(clock,
                  OMX_IndexConfigTimeClockState, &state);
              CHECKOMXERR(err);
              if (OMX_TIME_ClockStateRunning == state.eState) {
                in_head->nTimeStamp = state.nStartTime;
                OMX_LOGD("Invalid pts, Reset the first frame pts "TIME_FORMAT,
                    TIME_ARGS(in_head->nTimeStamp));
                return err;
              } else {
                OMX_LOGD("Invalid pts, clock eState %d, try again...", state.eState);
                usleep(5000);
                wait_count--;
                continue;
              }
            }
            OMX_LOGD("Invalid pts, reset pts failure, time out.");
          }
        }
      }
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::parseExtraData(
    OMX_BUFFERHEADERTYPE *in_head, CryptoInfo **pCryptoInfo) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 offset = (in_head->nOffset +in_head->nFilledLen + 3) / 4 * 4;
  OMX_OTHER_EXTRADATATYPE *extraData =
      reinterpret_cast<OMX_OTHER_EXTRADATATYPE *>(in_head->pBuffer + offset);
  OMX_OTHER_EXTRADATA_TIME_INFO *timeInfo = NULL;
  while (OMX_ExtraDataNone != extraData->eType) {
    if (static_cast<OMX_EXTRADATATYPE>(OMX_ExtraDataCryptoInfo) == extraData->eType) {
      CryptoInfo *hCryptoInfo = reinterpret_cast<CryptoInfo *>(extraData->data);
      OMX_U32 size = sizeof(CryptoInfo) + sizeof(CryptoInfo::SubSample) *
          (hCryptoInfo->mNumSubSamples - 1);
      *pCryptoInfo = reinterpret_cast<CryptoInfo *>(kdMalloc(size));
      if (*pCryptoInfo) {
        kdMemcpy(*pCryptoInfo, extraData->data, size);
      } else {
        OMX_LOGE("kdMalloc size %d fail", size);
      }
    } else if (static_cast<OMX_EXTRADATATYPE>(OMX_ExtraDataTimeInfo) ==
        extraData->eType) {
      timeInfo = reinterpret_cast<OMX_OTHER_EXTRADATA_TIME_INFO *>(
          extraData->data);
      if (mStreamPosition == 0 && timeInfo && in_head->nTimeStamp < 0 &&
          timeInfo->nDTSFromContainer >= 0) {
        OMX_LOGD("Modify timestamp from %lld to %lld by use dts",
            in_head->nTimeStamp, timeInfo->nDTSFromContainer +
            timeInfo->nMediatimeOffset);
        in_head->nTimeStamp = timeInfo->nDTSFromContainer +
            timeInfo->nMediatimeOffset;
      }
    }
    offset += extraData->nSize;
    extraData = reinterpret_cast<OMX_OTHER_EXTRADATATYPE *>(
        in_head->pBuffer + offset);
  }
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::processPRctrl(OMX_BUFFERHEADERTYPE *in_head,
    CryptoInfo **pCryptoInfo, OMX_U32 padding_size) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(in_head->pPlatformPrivate);
  if (NULL != *pCryptoInfo) {
    OMX_U64 encryptionOffset = 0;
    OMX_U64 iv = *(reinterpret_cast<OMX_U64 *>((*pCryptoInfo)->mIv));
    OMX_U32 ctrl_count = 0;
    OMX_U32 ctrl_start = mInputPort->getCtrlIndex();
    for (OMX_U32 i = 0; i < (*pCryptoInfo)->mNumSubSamples; i++) {
      OMX_U32 numBytesOfClearData =
          (*pCryptoInfo)->mSubSamples[i].mNumBytesOfClearData;
      OMX_U32 numBytesOfEncryptedData =
          (*pCryptoInfo)->mSubSamples[i].mNumBytesOfEncryptedData;
      if (0u < numBytesOfClearData) {
        mInputPort->updateCtrlInfo(numBytesOfClearData, OMX_FALSE, 0, 0);
        ctrl_count++;
      }
      if (0u < numBytesOfEncryptedData) {
        mInputPort->updateCtrlInfo(numBytesOfEncryptedData, OMX_TRUE, iv, encryptionOffset);
        encryptionOffset += numBytesOfEncryptedData;
        ctrl_count++;
      }
    }
    kdFree(*pCryptoInfo);
    *pCryptoInfo = NULL;
    if (padding_size) {
      mInputPort->updateCtrlInfo(padding_size, OMX_FALSE, 0, 0);
      mInputPort->mPaddingSize = 0;
      ctrl_count++;
    }
    amp_buffer->addCtrlInfoTag(ctrl_buf_num, ctrl_start, ctrl_count);
  } else {
    OMX_LOGV("Secure playready frame with No cryptoInfo...");
    OMX_U32 ctrl_start = mInputPort->getCtrlIndex();
    if (in_head->nFilledLen) {
      mInputPort->updateCtrlInfo(in_head->nFilledLen, OMX_FALSE, 0, 0);
      mInputPort->mPaddingSize = 0;
    }
    amp_buffer->addCtrlInfoTag(ctrl_buf_num, ctrl_start, 1);
  }
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::processWVctrl(OMX_BUFFERHEADERTYPE *in_head,
    CryptoInfo **pCryptoInfo, OMX_U32 padding_size) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(in_head->pPlatformPrivate);
  if (NULL != *pCryptoInfo) {
    OMX_U32 ctrl_count = 0;
    OMX_U32 ctrl_start = mInputPort->getCtrlIndex();
    for (OMX_U32 i = 0; i < (*pCryptoInfo)->mNumSubSamples; i++) {
      OMX_U32 numBytesOfClearData =
          (*pCryptoInfo)->mSubSamples[i].mNumBytesOfClearData;
      OMX_U32 numBytesOfEncryptedData =
          (*pCryptoInfo)->mSubSamples[i].mNumBytesOfEncryptedData;
      if (0u < numBytesOfClearData) {
        mInputPort->updateCtrlInfo(numBytesOfClearData, OMX_FALSE, NULL);
        ctrl_count++;
      }
      if (0u < numBytesOfEncryptedData) {
        mInputPort->updateCtrlInfo(numBytesOfEncryptedData, OMX_TRUE, (*pCryptoInfo)->mIv);
        ctrl_count++;
      }
    }
    kdFree(*pCryptoInfo);
    *pCryptoInfo = NULL;
    if (padding_size) {
      mInputPort->updateCtrlInfo(padding_size, OMX_FALSE, NULL);
      mInputPort->mPaddingSize = 0;
      ctrl_count++;
    }
    amp_buffer->addCtrlInfoTag(ctrl_buf_num, ctrl_start, ctrl_count);
  } else {
    OMX_LOGV("Secure widevine frame with No cryptoInfo...");
    OMX_U32 ctrl_start = mInputPort->getCtrlIndex();
    if (in_head->nFilledLen) {
      mInputPort->updateCtrlInfo(in_head->nFilledLen, OMX_FALSE, NULL);
      mInputPort->mPaddingSize = 0;
    }
    amp_buffer->addCtrlInfoTag(ctrl_buf_num, ctrl_start, 1);
  }
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::pushSecureDataToAmp (
    OMX_BUFFERHEADERTYPE *in_head, CryptoInfo **pCryptoInfo) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mInputPort->mAmpDisableTvp == OMX_TRUE) {
    OMX_U32 num_subsamples = 0;
    UINT32 num_bd = mInputPort->getBdChainNum();
    OMX_BOOL eos = in_head->nFlags & OMX_BUFFERFLAG_EOS ? OMX_TRUE : OMX_FALSE;
    if (NULL != *pCryptoInfo) {
      num_subsamples = (*pCryptoInfo)->mNumSubSamples;
    }
    if (num_bd >= (num_subsamples + 2) / 2) {
      if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
        OMX_LOGD("num_bd %d   num_subsamples %d", num_bd, num_subsamples);
      }
      OMX_U32 es_offset = mInputPort->getEsOffset();
      err = mInputPort->updateMemInfo(in_head);
      if (OMX_ErrorNone == err) {
        AMP_BD_ST *bd;
        mInputPort->popBdFromBdChain(&bd);
        mInputPort->clearBdTag(bd);
        if (mSchemeIdSend == OMX_FALSE) {
          mInputPort->addDmxCryptoCtrlInfoTag(bd, mStreamPosition);
          mSchemeIdSend = OMX_TRUE;
        }
        mInputPort->addDmxPtsTag(bd, in_head, mStreamPosition);
        if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
          OMX_LOGD("add pts tag, mStreamPosition %d", mStreamPosition);
        }
        if (num_subsamples > 0) {
          OMX_U32 datasize = 0;
          OMX_U64 encryptionOffset = 0;
          OMX_U64 initial_iv = *(reinterpret_cast<OMX_U64 *>((*pCryptoInfo)->mIv));
          OMX_U64 initial_offset = *(reinterpret_cast<OMX_U64 *>((*pCryptoInfo)->mIv + 8));
          OMX_U32 i;
          for (i = 0; i < num_subsamples; i++) {
            if ((i & 0x1) != 0) {
              mInputPort->addMemInfoTag(bd, es_offset, datasize, OMX_FALSE);
              if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
                OMX_LOGD("add mem info tag, es_offset %d  datasize %d",
                    es_offset, datasize);
                OMX_LOGD("push the bd %d of the encrypted frame to amp.", (i + 1) / 2);
              }
              es_offset += datasize;
              datasize = 0;
              pushAmpBd(AMP_PORT_INPUT, mInputObj.uiPortIdx, bd);
              mInputFrameNum++;
              mInputPort->popBdFromBdChain(&bd);
              mInputPort->clearBdTag(bd);
            }
            OMX_U32 numBytesOfClearData =
                (*pCryptoInfo)->mSubSamples[i].mNumBytesOfClearData;
            OMX_U32 numBytesOfEncryptedData =
                (*pCryptoInfo)->mSubSamples[i].mNumBytesOfEncryptedData;
            if (0u < numBytesOfClearData) {
              mInputPort->addDmxCtrlInfoTag(bd, mDataCount, numBytesOfClearData,
                  OMX_FALSE, 0, 0, 0, NULL);
              if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
                OMX_LOGD("add ctrl info tag, mDataCount %lld  size %d encrypted 0",
                    mDataCount, numBytesOfClearData);
              }
              datasize += numBytesOfClearData;
              mDataCount += numBytesOfClearData;
            }
            if (0u < numBytesOfEncryptedData) {
              if (kPlayReadyType == mInputPort->mDrmType) {
                mInputPort->addDmxCtrlInfoTag(bd, mDataCount, numBytesOfEncryptedData,
                    OMX_TRUE, initial_iv, encryptionOffset >> 4, encryptionOffset & 0x0f, NULL);
                if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
                  OMX_LOGD("add ctrl info tag, mDataCount %lld  size %d encrypted 1",
                      mDataCount, numBytesOfEncryptedData);
                }
                datasize += numBytesOfEncryptedData;
                mDataCount += numBytesOfEncryptedData;
                encryptionOffset += numBytesOfEncryptedData;
              } else if (kWideVineType == mInputPort->mDrmType) {
                mInputPort->addDmxCtrlInfoTag(bd, mDataCount, numBytesOfEncryptedData,
                    OMX_TRUE, 0, 0, 0, (*pCryptoInfo)->mIv);
                if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
                  OMX_LOGD("add ctrl info tag, mDataCount %lld  size %d encrypted 1",
                      mDataCount, numBytesOfEncryptedData);
                }
                datasize += numBytesOfEncryptedData;
                mDataCount += numBytesOfEncryptedData;
              } else if (kMarLinType == mInputPort->mDrmType) {
                OMX_U64 iv = initial_iv;
                OMX_U64 blockoffset = initial_offset;
                memReverse((OMX_U8 *)(&iv), sizeof(OMX_U64));
                memReverse((OMX_U8 *)(&blockoffset), sizeof(OMX_U64));
                blockoffset += (encryptionOffset >> 4);
                mInputPort->addDmxCtrlInfoTag(bd, mDataCount, numBytesOfEncryptedData,
                    OMX_TRUE, iv, blockoffset, encryptionOffset & 0x0f, NULL);
                if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
                  OMX_LOGD("add ctrl info tag, mDataCount %lld  size %d encrypted 1",
                      mDataCount, numBytesOfEncryptedData);
                }
                datasize += numBytesOfEncryptedData;
                mDataCount += numBytesOfEncryptedData;
                encryptionOffset += numBytesOfEncryptedData;
              } else {
                OMX_LOGE("Secure unknow drm type, can not be here, error happened...");
              }
            }
          }
          kdFree(*pCryptoInfo);
          *pCryptoInfo = NULL;
          OMX_U32 padding_size = mInputPort->mPaddingSize;
          if (padding_size) {
            mInputPort->addDmxCtrlInfoTag(bd, mDataCount, padding_size,
                OMX_FALSE, 0, 0, 0, NULL);
            if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
              OMX_LOGD("add ctrl info tag, mDataCount %lld  size %d encrypted 0",
                  mDataCount, padding_size);
            }
            datasize += padding_size;
            mDataCount += padding_size;
            mInputPort->mPaddingSize = 0;
          }
          mInputPort->addMemInfoTag(bd, es_offset, datasize, eos);
          if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
            OMX_LOGD("add mem info tag, es_offset %d  datasize %d",
                es_offset, datasize);
            OMX_LOGD("push the bd %d of the encrypted frame to amp.", (i + 2) / 2);
          }
          if (mTsRecover) {
            mTsRecover->insertTimeStamp(in_head->nTimeStamp);
          }
          pushAmpBd(AMP_PORT_INPUT, mInputObj.uiPortIdx, bd);
          mInputFrameNum++;
        } else {
          OMX_LOGV("Secure frame with No cryptoInfo...");
          if (in_head->nFilledLen) {
            mInputPort->addDmxCtrlInfoTag(bd, mDataCount, in_head->nFilledLen,
                OMX_FALSE, 0, 0, 0, NULL);
            if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
              OMX_LOGD("add ctrl info tag, mDataCount %lld  size %d encrypted 0",
                  mDataCount, in_head->nFilledLen);
            }
            mDataCount += in_head->nFilledLen;
            mInputPort->mPaddingSize = 0;
            mInputPort->addMemInfoTag(bd, es_offset, in_head->nFilledLen, eos);
            if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
              OMX_LOGD("add mem info tag, es_offset %d  datasize %d",
                  es_offset, in_head->nFilledLen);
              OMX_LOGD("push the bd 1 of the clear frame to amp.");
            }
            if (mTsRecover) {
              mTsRecover->insertTimeStamp(in_head->nTimeStamp);
            }
            pushAmpBd(AMP_PORT_INPUT, mInputObj.uiPortIdx, bd);
            mInputFrameNum++;
          }
        }
        mStreamPosition += in_head->nFilledLen;
        // For Tvp2.0, the stream position will be masked in DMX, so do not need to mask here.
        //mStreamPosition &= kStreamPositionMask;
        returnBuffer(mInputPort, in_head);
        mCachedhead = NULL;
      } else {
        OMX_LOGV("Secure cbuf Lack of mem, waitting.");
      }
    } else {
      OMX_LOGV("Secure bdChain lack of bd, waitting.");
    }
  } else {
    AMP_BD_ST *bd = NULL;
    mInputPort->popBdFromBdChain(&bd);
    if (NULL != bd) {
      AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(in_head->pPlatformPrivate);
      amp_buffer->setBD(bd);
      amp_buffer->clearBdTag();
      amp_buffer->addPtsTag(in_head, mStreamPosition);
      OMX_U32 es_offset = mInputPort->getEsOffset();
      err = mInputPort->updateMemInfo(in_head);
      if (OMX_ErrorNone == err) {
        OMX_U32 padding_size = mInputPort->mPaddingSize;
        amp_buffer->addMemInfoTag(in_head, es_offset);
        if (kPlayReadyType == mInputPort->mDrmType) {
          processPRctrl(in_head, pCryptoInfo, padding_size);
        } else if (kWideVineType == mInputPort->mDrmType) {
          processWVctrl(in_head, pCryptoInfo, padding_size);
        } else {
          OMX_LOGE("Secure unknow drm type, can not be here, error happened...");
        }
        mStreamPosition += in_head->nFilledLen;
        mStreamPosition &= kStreamPositionMask;
        if (mTsRecover) {
          mTsRecover->insertTimeStamp(in_head->nTimeStamp);
        }
        pushAmpBd(AMP_PORT_INPUT, 0, amp_buffer->getBD());
        returnBuffer(mInputPort, in_head);
        mInputFrameNum++;
        mCachedhead = NULL;
      } else {
        OMX_LOGV("Secure cbuf Lack of mem, waitting.");
        mInputPort->pushBdToBdChain(bd);
      }
    } else {
      OMX_LOGV("Secure bdChain lack of bd, waitting.");
    }
  }
  return err;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::writeDataToCbuf(
    OMX_BUFFERHEADERTYPE *in_head, OMX_BOOL isCaching) {
  OMX_U32 ret  = AMPCBUF_SUCCESS;
  OMX_U32 flag = (mEOS == OMX_TRUE) ? AMP_MEMINFO_FLAG_EOS_MASK : 0;
#ifdef VDEC_USE_CBUF_STREAM_IN_MODE
  if (!isCaching) {
    mInputPort->formatEsData(in_head, OMX_FALSE, OMX_FALSE);
  }
  if (flag & AMP_MEMINFO_FLAG_EOS_MASK && !isCaching) {
    //padding 64k to enable the vdec handle eos event.
    OMX_U32 stream_pad_size = (mStreamPosition + in_head->nFilledLen) & 63 ?
        64 - (mStreamPosition + in_head->nFilledLen) & 63 : 0;
    kdMemcpy(in_head->pBuffer + in_head->nOffset + in_head->nFilledLen,
        eos_padding, 32);
    kdMemset(in_head->pBuffer + in_head->nOffset + in_head->nFilledLen + 32,
        0x88, stream_in_eos_padding_size - 32 + stream_pad_size);
    in_head->nFilledLen += stream_in_eos_padding_size + stream_pad_size;
  }
  UINT32 datasize;
  AMP_CBUFGetWrPtr(mPool, NULL, NULL, NULL, &datasize);
  if (stream_in_max_push_size >= datasize + in_head->nFilledLen) {
    ret = AMP_CBUFWriteData(mPool, in_head->pBuffer + in_head->nOffset,
        in_head->nFilledLen, flag, NULL);
    if (AMPCBUF_SUCCESS == ret) {
      // Because the free space of the Bd is only 512 Bytes, the Num of pts tag in one Bd
      // is limited to 12.
      if (mAddedPtsTagNum <= stream_in_max_pts_tag_num) {
        AMP_BDTAG_UNITSTART pts_tag;
        OMX_U32 offset = mStreamPosition % stream_in_buf_size;
        kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_UNITSTART));
        pts_tag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL, sizeof(AMP_BDTAG_UNITSTART)};
        convertUsto90K(in_head->nTimeStamp, &pts_tag.uPtsHigh, &pts_tag.uPtsLow);
        pts_tag.uStrmPos = mStreamPosition;
        AMP_CBUFInsertTag(mPool, reinterpret_cast<UINT8 *>(&pts_tag), offset);
        mAddedPtsTagNum++;
      }
      if (mTsRecover) {
        mTsRecover->insertTimeStamp(in_head->nTimeStamp);
      }
      mStreamPosition += in_head->nFilledLen;
      mStreamPosition &= kStreamPositionMask;
      returnBuffer(mInputPort, in_head);
      mCachedhead = NULL;
      mInputFrameNum++;
      mInBDBackNum++;
    } else if (AMPCBUF_LACKSPACE == ret) {
      OMX_LOGV("Cbuf Lack of space.");
    } else {
      OMX_LOGE("Cbuf already Reached Eos!");
      returnBuffer(mInputPort, in_head);
      mCachedhead = NULL;
      mInputFrameNum++;
      mInBDBackNum++;
    }
  } else {
    ret = AMPCBUF_ERR;
    OMX_LOGV("Cbuf meet the max push size.");
  }
#else
  UINT32 offset = 0;
  ret = AMP_CBUFWriteData(mPool, in_head->pBuffer + in_head->nOffset,
      in_head->nFilledLen, flag, &offset);
  if (AMPCBUF_SUCCESS == ret) {
    AMP_BDTAG_UNITSTART pts_tag;
    kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_UNITSTART));
    pts_tag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL, sizeof(AMP_BDTAG_UNITSTART)};
    convertUsto90K(in_head->nTimeStamp, &pts_tag.uPtsHigh, &pts_tag.uPtsLow);
    pts_tag.uStrmPos = mStreamPosition;
    AMP_CBUFInsertTag(mPool, reinterpret_cast<UINT8 *>(&pts_tag), offset);
    if (mTsRecover) {
      mTsRecover->insertTimeStamp(in_head->nTimeStamp);
    }
    mStreamPosition += in_head->nFilledLen;
    mStreamPosition &= kStreamPositionMask;
    if (OMX_TRUE == mIsWMDRM) {
      DecryptSecureDataToCbuf(in_head, &mCryptoInfo, offset);
    }
    returnBuffer(mInputPort, in_head);
    mCachedhead = NULL;
    mInputFrameNum++;
    mInBDBackNum++;
  } else if (AMPCBUF_LACKSPACE == ret) {
    OMX_LOGV("Cbuf Lack of space.");
  } else {
    OMX_LOGE("Cbuf already Reached Eos!");
    returnBuffer(mInputPort, in_head);
    mCachedhead = NULL;
    mInputFrameNum++;
    mInBDBackNum++;
  }
#endif
  return static_cast<OMX_ERRORTYPE>(ret);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::pushCbufDataToAmp() {
  UINT32 ret = AMPCBUF_SUCCESS;
  AMP_BD_ST *bd;
  UINT32 offset, framesize;
  ret = AMP_CBUFRequest(mPool, &bd, &offset, &framesize);
  if (AMPCBUF_SUCCESS == ret) {
    mPushedBdNum++;
    mAddedPtsTagNum = 0;
    if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN) == OMX_TRUE) {
      OMX_LOGD("Cbuf <%u/%u> Push Bd to Amp: offset %u  size %u",
          mReturnedBdNum, mPushedBdNum, offset, framesize);
    }
    pushAmpBd(AMP_PORT_INPUT, 0, bd);
  } else if (AMPCBUF_LACKBD == ret) {
    OMX_LOGV("Cbuf Lack of Bd, waitting.");
  } else if (AMPCBUF_LACKDATA == ret) {
    OMX_LOGV("Cbuf Lack of Data, waitting.");
  } else {
    OMX_LOGE("Cbuf Request Bd err %d.", ret);
  }
  return static_cast<OMX_ERRORTYPE>(ret);
}

OMX_ERRORTYPE OmxAmpVideoDecoder::DecryptSecureDataToCbuf(
    OMX_BUFFERHEADERTYPE *in_head,
    CryptoInfo **pCryptoInfo,
    OMX_U32 cbuf_offset) {
  HRESULT err = SUCCESS;
#ifndef DISABLE_PLAYREADY
  if (NULL != *pCryptoInfo) {
    OMX_U32 head_offset = 0;
    OMX_U32 total_subsample_size = 0;
    for (OMX_U32 i = 0; i < (*pCryptoInfo)->mNumSubSamples; i++) {
      OMX_U32 numBytesOfClearData =
          (*pCryptoInfo)->mSubSamples[i].mNumBytesOfClearData;
      OMX_U32 numBytesOfEncryptedData =
          (*pCryptoInfo)->mSubSamples[i].mNumBytesOfEncryptedData;
      if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
        OMX_LOGD("Subsample %d  clear %d  encrypted %d", i, numBytesOfClearData,
            numBytesOfEncryptedData);
      }
      if (0u < numBytesOfClearData + mInputPort->mPrePaddingSize) {
      #ifdef WMDRM_SUPPORT
        err = MV_DRM_WMDRM_Content_Decrypt(NULL,
            (char *)(in_head->pBuffer + in_head->nOffset + head_offset),
            numBytesOfClearData + mInputPort->mPrePaddingSize,
            0,
            mShmHandle,
            cbuf_offset);
      #endif
        if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
          OMX_LOGD("Decrypt clear  offset %d  size %d", cbuf_offset,
              numBytesOfClearData + mInputPort->mPrePaddingSize);
        }
        if (SUCCESS != err) {
          OMX_LOGE("Decrypt clear data err %d", err);
        }
        head_offset += numBytesOfClearData + mInputPort->mPrePaddingSize;
        cbuf_offset += numBytesOfClearData + mInputPort->mPrePaddingSize;
        total_subsample_size += numBytesOfClearData + mInputPort->mPrePaddingSize;
        mInputPort->mPrePaddingSize = 0;
      }
      if (0u < numBytesOfEncryptedData) {
      #ifdef WMDRM_SUPPORT
        err = MV_DRM_WMDRM_Content_Decrypt(NULL,
            (char *)(in_head->pBuffer + in_head->nOffset + head_offset),
            numBytesOfEncryptedData,
            1,
            mShmHandle,
            cbuf_offset);
      #endif
        if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
          OMX_LOGD("Decrypt encrypted  offset %d  size %d", cbuf_offset,
              numBytesOfEncryptedData);
        }
        if (SUCCESS != err) {
          OMX_LOGE("Decrypt clear data err %d", err);
        }
        head_offset += numBytesOfEncryptedData;
        cbuf_offset += numBytesOfEncryptedData;
        total_subsample_size += numBytesOfEncryptedData;
      }
    }
    kdFree(*pCryptoInfo);
    *pCryptoInfo = NULL;
    if (mInputPort->mPaddingSize) {
    #ifdef WMDRM_SUPPORT
      err = MV_DRM_WMDRM_Content_Decrypt(NULL,
          (char *)(in_head->pBuffer + in_head->nOffset + head_offset),
          mInputPort->mPaddingSize,
          0,
          mShmHandle,
          cbuf_offset);
    #endif
      if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
        OMX_LOGD("Decrypt clear  offset %d  size %d", cbuf_offset, mInputPort->mPaddingSize);
      }
      if (SUCCESS != err) {
        OMX_LOGE("Decrypt clear data err %d", err);
      }
      head_offset += mInputPort->mPaddingSize;
      cbuf_offset += mInputPort->mPaddingSize;
      total_subsample_size += mInputPort->mPaddingSize;
      mInputPort->mPaddingSize = 0;
    }
    if (total_subsample_size != in_head->nFilledLen) {
      OMX_LOGE("The total subsample size %d is not equal to the buffer FilledLen %d.",
          total_subsample_size, in_head->nFilledLen);
    }
  } else {
    OMX_LOGV("Secure WM frame with No cryptoInfo...");
    #ifdef WMDRM_SUPPORT
    err = MV_DRM_WMDRM_Content_Decrypt(NULL,
        (char *)(in_head->pBuffer + in_head->nOffset),
        in_head->nFilledLen,
        0,
        mShmHandle,
        cbuf_offset);
    #endif
    if (CheckMediaHelperEnable(mMediaHelper, VIDEO_SECURE_INFO) == OMX_TRUE) {
      OMX_LOGD("Decrypt clear  offset %d  size %d", cbuf_offset, in_head->nFilledLen);
    }
    cbuf_offset += in_head->nFilledLen;
    if (SUCCESS != err) {
      OMX_LOGE("Decrypt clear data err %d", err);
    }
  }
#endif
  return static_cast<OMX_ERRORTYPE>(err);
}

HRESULT OmxAmpVideoDecoder::processInputDone(AMP_BD_ST *bd) {
  HRESULT err = SUCCESS;
  if (NULL != mPool) {
    OMX_U32 ret;
    ret = AMP_CBUFRelease(mPool, bd);
    if (AMPCBUF_SUCCESS == ret) {
      mReturnedBdNum++;
      if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN_BACK) == OMX_TRUE) {
        OMX_LOGD("Cbuf [%u/%u] Return Bd from Amp", mReturnedBdNum, mPushedBdNum);
      }
    } else {
      OMX_LOGE("Cbuf Release Bd error %u.", ret);
    }
    return err;
  }
  if (OMX_TRUE == mInputPort->mIsDrm) {
    UINT32 i, bd_num;
    void *tag;
    err = AMP_BDTAG_GetNum(bd, &bd_num);
    CHECKAMPERRLOG(err, "get bd tag num.");
    for (i = 0; i < bd_num; i++) {
      err = AMP_BDTAG_GetWithIndex(bd, i, &tag);
      if (SUCCESS != err) {
        OMX_LOGE("Failed to get tag %d from bd 0x%x.", i, bd->uiBDId);
        return err;
      }
      if (mInputPort->mAmpDisableTvp == OMX_TRUE) {
        if (static_cast<AMP_BDTAG_MEMINFO *>(tag)->Header.eType ==
            AMP_BDTAG_ASSOCIATE_MEM_INFO) {
          mInputPort->updatePushedBytes(static_cast<AMP_BDTAG_MEMINFO *>(tag)->uSize);
        }
      } else {
        if (static_cast<AMP_BDTAG_MEMINFO *>(tag)->Header.eType ==
            AMP_BDTAG_ASSOCIATE_MEM_INFO) {
          mInputPort->updatePushedBytes(static_cast<AMP_BDTAG_MEMINFO *>(tag)->uSize);
        } else if (static_cast<AMP_BDTAG_TVPCTRLBUF *>(tag)->Header.eType ==
            AMP_BDTAG_PAYLOAD_CTRL_BUFFER) {
          mInputPort->updatePushedCtrls(static_cast<AMP_BDTAG_MEMINFO *>(tag)->uSize);
        }
      }
    }
    mInputPort->pushBdToBdChain(bd);
    mInBDBackNum++;
    if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN_BACK) == OMX_TRUE) {
      OMX_LOGD("[In] %d/%d", mInBDBackNum, mInputFrameNum);
    }
    return err;
  }
  OMX_BUFFERHEADERTYPE *buf_header = mInputPort->getBufferHeader(bd);
  if (NULL != buf_header) {
    returnBuffer(mInputPort, buf_header);
    mInBDBackNum++;
    if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN_BACK) == OMX_TRUE) {
      OMX_LOGD("[In] %d/%d", mInBDBackNum, mInputFrameNum);
    }
  }
  return err;
}

HRESULT OmxAmpVideoDecoder::processOutputDone(AMP_BD_ST *bd) {
  HRESULT err = SUCCESS;
  OMX_BUFFERHEADERTYPE *buf_header = mOutputPort->getBufferHeader(bd);
  if (NULL != buf_header) {
    AmpBuffer *amp_buffer =  static_cast<AmpBuffer *>(buf_header->pPlatformPrivate);
    AMP_BDTAG_MEMINFO *mem_info = NULL;
    err = amp_buffer->getMemInfo(&mem_info);
    if (mem_info == NULL) {
      OMX_LOGE("amp_buffer get getMemInfo() return 0x%x", err);
      return err;
    }
    if (mem_info->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
      OMX_LOGD("[Out] Receive EOS");
      buf_header->nFlags |= OMX_BUFFERFLAG_EOS;
      mem_info->uFlag = 0;
      mOutputEOS = OMX_TRUE;
      postEvent(OMX_EventBufferFlag, buf_header->nOutputPortIndex,
          OMX_BUFFERFLAG_EOS);
    }
    if (mMark.hMarkTargetComponent != NULL) {
      buf_header->hMarkTargetComponent = mMark.hMarkTargetComponent;
      buf_header->pMarkData = mMark.pMarkData;
      mMark.hMarkTargetComponent = NULL;
      mMark.pMarkData = NULL;
    }
    AMP_BGTAG_FRAME_INFO *frame_info = NULL;
    err = amp_buffer->getFrameInfo(&frame_info);
    OMX_TICKS pts_low, pts_high;
    OMX_U32 framesize;
    if (frame_info) {
      pts_low = static_cast<OMX_TICKS>(frame_info->uiPtsLow);
      pts_high = static_cast<OMX_TICKS>(frame_info->uiPtsHigh);
      if (frame_info->uiPtsHigh & TIME_STAMP_VALID_MASK) {
        if (mTsRecover) {
          mTsRecover->findTimeStamp(pts_high, pts_low, &buf_header->nTimeStamp);
        } else {
          buf_header->nTimeStamp = convert90KtoUs(pts_high, pts_low);
        }
      }
      framesize = getNativeBufferSize(mOutputPort->mDefinition.format.video.eColorFormat,
          frame_info->uiContentW, frame_info->uiContentH);
    } else {
      OMX_LOGE("Get getFrameInfo return 0x%x", err);
    }
    if (OMX_TRUE == mOutputEOS) {
      buf_header->nFilledLen = 0;
    } else {
      if (mOutputPort->mStoreMetaDataInBuffers) {
        buf_header->nFilledLen = buf_header->nAllocLen;
      } else {
        buf_header->nFilledLen = framesize;
      }
    }
    if (mOutputFrameNum == 0) {
      buf_header->nFlags |= OMX_BUFFERFLAG_STARTTIME;
    }
    if (mOutputPort->mStoreMetaDataInBuffers) {
      GraphicBufferMapper::get().unlock((buffer_handle_t)(amp_buffer->getGfx()));
    } else if (mUseNativeBuffers) {
      ((GraphicBuffer *)(amp_buffer->getGfx()))->unlock();
    }
    returnBuffer(mOutputPort, buf_header);
    mOutputFrameNum++;
    if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_OUT_BACK) == OMX_TRUE) {
      OMX_LOGD("[Out] %d/%d, size %d, pts "TIME_FORMAT" flag %x",
          mOutputFrameNum, mOutBDPushNum, buf_header->nFilledLen,
          TIME_ARGS(buf_header->nTimeStamp), buf_header->nFlags);
    }
  } else {
    OMX_LOGE("Output bd (bdid = %d) is not recognized.", bd->uiBDId);
  }
  return err;
}

HRESULT OmxAmpVideoDecoder::pushBufferDone(
    AMP_COMPONENT component,
    AMP_PORT_IO port_Io,
    UINT32 port_idx,
    AMP_BD_ST *bd, void *context) {
  HRESULT err = SUCCESS;
  OMX_BOOL isLastOutFrame = OMX_FALSE;
  OmxAmpVideoDecoder *decoder = static_cast<OmxAmpVideoDecoder *>(context);
  if (CheckMediaHelperEnable(decoder->mMediaHelper,
      PUSH_BUFFER_DONE) == OMX_TRUE) {
    OMX_LOGD("pushBufferDone, port_IO %d, bd 0x%x, bd.bdid 0x%x, bd.allocva 0x%x",
        port_Io, bd, bd->uiBDId, bd->uiAllocVA);
  }
  if (AMP_PORT_INPUT == port_Io && 0 == port_idx) {
    err = decoder->processInputDone(bd);
    if (SUCCESS != err) {
      OMX_LOGE("processInputDone err %d", err);
    }
  } else if (AMP_PORT_OUTPUT == port_Io && 0 == port_idx) {
    err = decoder->processOutputDone(bd);
    if (SUCCESS != err) {
      OMX_LOGE("processOutputDone err %d", err);
    }
  } else {
    OMX_LOGE("bd not be used");
    err = ERR_NOTIMPL;
  }
  return err;
}

void OmxAmpVideoDecoder::updateAmpBuffer(AmpBuffer *amp_buffer,
       buffer_handle_t handle) {
  OMX_LOGD("updateAmpBuffer");
  amp_buffer->setGfx((void *)handle);
  OMX_VIDEO_PORTDEFINITIONTYPE &video_def = mOutputPort->getVideoDefinition();
  Rect rect(video_def.nFrameWidth, video_def.nFrameHeight);
  void *gid = NULL;
  uint32_t usage = GraphicBuffer::USAGE_HW_RENDER | GRALLOC_USAGE_PRIVATE_0;
  AMP_SHM_HANDLE shm;
  AMP_SHM_TYPE type;
  if (mInputPort->mIsDrm || mInputPort->mOmxDisableTvp) {
    type = AMP_SHM_TYPE_GEN(AMP_SHM_CLASS_GRAPHICS, AMP_SHM_FLAG_CACHEABLE
        | AMP_SHM_FLAG_DYNAMIC | AMP_SHM_FLAG_SECURE);
  } else {
    type = AMP_SHM_TYPE_GEN(AMP_SHM_CLASS_GRAPHICS, AMP_SHM_FLAG_CACHEABLE
        | AMP_SHM_FLAG_DYNAMIC);
  }
  GraphicBufferMapper::get().lock(handle, usage, rect, &gid);
  HRESULT err = AMP_SHM_Import((int)gid, type, 32, &shm);
  if (err != SUCCESS) {
    OMX_LOGE("AMP_SHM_Import error %d.", err);
  }
  GraphicBufferMapper::get().unlock(handle);
  amp_buffer->setShm(shm);
  amp_buffer->updateMemInfo(shm);
}

void OmxAmpVideoDecoder::pushBufferLoop() {
  OMX_BUFFERHEADERTYPE *in_head = NULL, *out_head = NULL;
  OMX_BOOL isCaching = OMX_FALSE;
Loop_start:
  OMX_LOGD("Start decoding loop");
  while (!mShouldExit) {
    //Get input Buffer
    kdThreadMutexLock(mBufferLock);
    if (!mInputPort->isEmpty() || NULL != mCachedhead) {
      if (NULL != mCachedhead) {
        in_head = mCachedhead;
        isCaching = OMX_TRUE;
        OMX_LOGV("Use the cached input buffer %p", in_head);
      } else {
        in_head = mInputPort->popBuffer();
        mCachedhead = in_head;
        isCaching = OMX_FALSE;
        OMX_LOGV("Got input buffer %p", in_head);
      }
      if (NULL != in_head) {
        if (mInputPort->isFlushing()) {
          OMX_LOGD("Return input buffer when flushing");
          returnBuffer(mInputPort, in_head);
          in_head = NULL;
          mCachedhead = NULL;
          kdThreadMutexUnlock(mBufferLock);
          continue;
        }
        // Process start frame
        if (in_head->nFlags & OMX_BUFFERFLAG_STARTTIME) {
        }
        if (in_head->nFlags & OMX_BUFFERFLAG_EOS) {
          OMX_LOGD("Meet EOS Frame, wait for stop");
          mEOS = OMX_TRUE;
        }
        if (in_head->hMarkTargetComponent != NULL) {
          mMark.hMarkTargetComponent = in_head->hMarkTargetComponent;
          mMark.pMarkData = in_head->pMarkData;
        }
        // For Marlin, some CSD data has been padded at the beginning of the media
        // frame, so we move it here.
        if (!mInputPort->mOmxDisableTvp && !isCaching) {
          if ((mOutputPort->isTunneled() && (in_head->nFlags & OMX_BUFFERFLAG_EXTRADATA))
              || (!mOutputPort->isTunneled() && mInputPort->mIsDrm))
          parseExtraData(in_head, &mCryptoInfo);
        }
        // Process codec config data
        if (mCryptoInfo == NULL && (in_head->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {
          if (!mInputPort->mOmxDisableTvp) {
            char *pData = reinterpret_cast<char *>(in_head->pBuffer)
                + in_head->nOffset;
            mExtraData->add(pData, in_head->nFilledLen);
          } else {
            mSecureExtraData->add(in_head->pBuffer, in_head->nFilledLen, in_head->nOffset);
          }
          in_head->nFilledLen = 0;
          in_head->nOffset = 0;
          returnBuffer(mInputPort, in_head);
          mCachedhead = NULL;
          mInited = OMX_FALSE;
          kdThreadMutexUnlock(mBufferLock);
          continue;
        }
        if (mInited == OMX_FALSE) {
          OMX_U32 codec_data_size;
          if (mInputPort->getVideoDefinition().eCompressionFormat != OMX_VIDEO_CodingHEVC) {
            if (!mInputPort->mOmxDisableTvp) {
              codec_data_size = mExtraData->getExtraDataSize();
              OMX_U8 *data = in_head->pBuffer + in_head->nOffset;
              kdMemmove(data + codec_data_size, data, in_head->nFilledLen);
              mExtraData->merge(data, codec_data_size);
            } else {
              void *addr = NULL;
              codec_data_size = mSecureExtraData->getExtraDataSize();
              if (AMP_SHM_GetPhysicalAddress((AMP_SHM_HANDLE)in_head->pBuffer,
                  in_head->nOffset, &addr)) {
                OMX_LOGE("AMP_SHM_GetPhysicalAddress error.");
              }
              if (TEEC_FastMemMove(&mContext, addr + codec_data_size, addr, in_head->nFilledLen)) {
                OMX_LOGE("TEEU_MemCopy error.");
              }
              mSecureExtraData->merge(in_head->pBuffer, codec_data_size, in_head->nOffset);
            }
            in_head->nFilledLen += codec_data_size;
            if (NULL != mCryptoInfo) {
              mCryptoInfo->mSubSamples[0].mNumBytesOfClearData += codec_data_size;
            }
            mInputPort->setCodecDataSize(codec_data_size);
          }
          mInited = OMX_TRUE;
        }
        if (in_head->nTimeStamp < 0 && mStreamPosition == 0) {
            resetPtsForFirstFrame(in_head);
        }
        if (!isCaching && CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN) == OMX_TRUE) {
          OMX_LOGD("<In> %d/%d, size %d\tpts "TIME_FORMAT "\tflag %s",
              mInBDBackNum, mInputFrameNum, in_head->nFilledLen,
              TIME_ARGS(in_head->nTimeStamp),
              GetOMXBufferFlagDescription(in_head->nFlags));
        }
        // dump es data
        if (!mInputPort->mOmxDisableTvp && !isCaching
            && CheckMediaHelperEnable(mMediaHelper, DUMP_VIDEO_ES) == OMX_TRUE) {
          mediainfo_dump_data(MEDIA_VIDEO, MEDIA_DUMP_ES,
              in_head->pBuffer + in_head->nOffset, in_head->nFilledLen);
        }
        if (OMX_FALSE == mInputPort->mIsDrm) {
          if (NULL != mPool) {
            // 1. for tunnel mode non-secure video.
            // 2. for WMDRM play, but just support frame in mode.
#ifdef VDEC_USE_CBUF_STREAM_IN_MODE
            writeDataToCbuf(in_head, isCaching);
#else
            OMX_U32 ret = SUCCESS;
            // HEVC don't need pad
            if (mInputPort->getVideoDefinition().eCompressionFormat !=
                OMX_VIDEO_CodingHEVC && !isCaching) {
              mInputPort->formatEsData(in_head, OMX_TRUE, OMX_FALSE);
            }
            ret = pushCbufDataToAmp();
            if (AMPCBUF_LACKBD != ret) {
              writeDataToCbuf(in_head, isCaching);
            }
            if (mEOS == OMX_TRUE) {
              pushCbufDataToAmp();
            }
#endif
          } else {
            // for non-tunnel mode video.
            AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(in_head->pPlatformPrivate);
            amp_buffer->updatePts(in_head, mStreamPosition);
            if (mTsRecover && (in_head->nFlags & OMX_BUFFERFLAG_EOS) == 0) {
              mTsRecover->insertTimeStamp(in_head->nTimeStamp);
            }
            if (!mInputPort->mOmxDisableTvp) {
              mInputPort->formatEsData(in_head, OMX_TRUE, OMX_FALSE);
            } else {
              mInputPort->formatEsData(in_head, OMX_TRUE, OMX_TRUE);
            }
            amp_buffer->updateMemInfo(in_head);
            mStreamPosition += in_head->nFilledLen;
            mStreamPosition &= kStreamPositionMask;
            pushAmpBd(AMP_PORT_INPUT, 0, amp_buffer->getBD());
            mInputFrameNum++;
            mCachedhead = NULL;
          }
        } else {
          // for tunnel mode secure video.
          pushSecureDataToAmp(in_head, &mCryptoInfo);
        }
      } else {
        OMX_LOGE("get input buffer %p error",in_head);
      }
    }
#ifdef VDEC_USE_CBUF_STREAM_IN_MODE
    // request and push bd to amp when use stream in mode.
    if (NULL != mPool) {
      // for tunnel mode non-secure video.
      pushCbufDataToAmp();
    }
#endif
    // Get output buffer
    if (mStartPushOutBuf && !mOutputPort->isEmpty() && mOutputEOS == OMX_FALSE) {
      if (!mHasRegisterOutBuf) {
        registerBds(mOutputPort);
        mHasRegisterOutBuf = OMX_TRUE;
      }
      out_head = mOutputPort->popBuffer();
      OMX_LOGV("Got output buffer %p", out_head);
      if (NULL != out_head) {
        if (mOutputPort->isFlushing()) {
          OMX_LOGD("Return output buffer when flushing");
          returnBuffer(mOutputPort, out_head);
          kdThreadMutexUnlock(mBufferLock);
          continue;
        }
        AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
            out_head->pPlatformPrivate);
        mOutBDPushNum++;
        if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_OUT) == OMX_TRUE) {
          OMX_LOGD("<Out> %d/%d", mOutputFrameNum, mOutBDPushNum);
        }
        if (mOutputPort->mStoreMetaDataInBuffers) {
#if PLATFORM_SDK_VERSION >= 19
          android::VideoDecoderOutputMetaData *metadata =
            reinterpret_cast<android::VideoDecoderOutputMetaData *>(out_head->pBuffer);
          if (metadata->eType != kMetadataBufferTypeGrallocSource) {
            OMX_LOGE("wrong buffer type.");
            returnBuffer(mOutputPort, out_head);
            kdThreadMutexUnlock(mBufferLock);
            continue;
          }
          if (amp_buffer->getGfx() != metadata->pHandle) {
            updateAmpBuffer(amp_buffer, metadata->pHandle);
          }
          uint32_t usage = GraphicBuffer::USAGE_HW_RENDER;
          OMX_VIDEO_PORTDEFINITIONTYPE &video_def = mOutputPort->getVideoDefinition();
          Rect rect(video_def.nFrameWidth, video_def.nFrameHeight);
          void *buf = NULL;
          GraphicBufferMapper::get().lock(metadata->pHandle, usage, rect, &buf);
#endif
        } else if (mUseNativeBuffers) {
          uint32_t usage = GraphicBuffer::USAGE_HW_RENDER;
          void *buf = NULL;
          ((GraphicBuffer *)(amp_buffer->getGfx()))->lock(usage, &buf);
        }
        pushAmpBd(AMP_PORT_OUTPUT, 0, amp_buffer->getBD());
      } else {
        OMX_LOGE("get output buffer %p error",out_head);
      }
    }
    kdThreadMutexUnlock(mBufferLock);
    usleep(5000);
  } // end of while (!mShouldExit)

  kdThreadMutexLock(mPauseLock);
  if (mPaused) {
    kdThreadCondWait(mPauseCond, mPauseLock);
    kdThreadMutexUnlock(mPauseLock);
    goto Loop_start;
  }
  kdThreadMutexUnlock(mPauseLock);
}

void* OmxAmpVideoDecoder::threadEntry(void * args) {
  OmxAmpVideoDecoder *decoder = (OmxAmpVideoDecoder *)args;
  prctl(PR_SET_NAME, "OmxVideoDecoder", 0, 0, 0);
  decoder->pushBufferLoop();
  return NULL;
}

OMX_ERRORTYPE OmxAmpVideoDecoder::createAmpVideoDecoder(
    OMX_HANDLETYPE *handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE *callBacks) {
  OmxAmpVideoDecoder *comp = new  OmxAmpVideoDecoder(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}

OMX_S32 OmxAmpVideoDecoder::bindPQSource() {
  OMX_LOG_FUNCTION_ENTER;
  //Bind PQ sourceID with videoplane;
  String8 FILE_SOURCE_URI;
  if ( -1 == mSourceId) {
    mVideoPlaneId = getVideoPlane();
    OMX_LOGD("mVideoPlaneId = %d", mVideoPlaneId);
    if ((-1 != mVideoPlaneId) && (mSourceControl != NULL)) {
     if (0 == mVideoPlaneId) {
       FILE_SOURCE_URI = String8("file://localhost?plane=0");
     } else {
       FILE_SOURCE_URI = String8("file://localhost?plane=1");
     }
     mSourceControl->getSourceByUri(FILE_SOURCE_URI, &mSourceId);
     if ( -1 != mSourceId) {
       OMX_LOGD("get PQ sourceid :%d, sourceURL :%s", mSourceId, FILE_SOURCE_URI.string());
       status_t ret = mSourceControl->setSource(mSourceId, mVideoPlaneId);
       if (OK != ret) {
         OMX_LOGE("setSource() through source control failed!");
         mSourceId = -1;
       }
      }
     } else {
       OMX_LOGD("blind PQ source with videoplane failed.");
     }
   }
  OMX_LOG_FUNCTION_ENTER;
  return mSourceId;
}

OMX_S32 OmxAmpVideoDecoder::getVideoPlane() {
  OMX_LOG_FUNCTION_ENTER;
  OmxPortImpl *port = getPort(kVideoPortStartNumber + 1);
  if ((NULL != port) && (port->isTunneled())) {
    OmxComponentImpl *pComp =  static_cast<OmxComponentImpl *>(
      static_cast<OMX_COMPONENTTYPE *>(port->getTunnelComponent())->pComponentPrivate);
    if (NULL != strstr(pComp->mName, "iv_processor")) {
      return static_cast<OmxVoutProxy *>(pComp)->getVideoPlane();
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return -1;
}

HRESULT OmxAmpVideoDecoder::registerEvent(AMP_EVENT_CODE event) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mListener) {
    err = AMP_Event_RegisterCallback(mListener, event, eventHandle, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "register VDEC notify");

    if (!err) {
      AMP_RPC(err, AMP_VDEC_RegisterNotify, mAmpHandle,
          AMP_Event_GetServiceID(mListener), event);
      CHECKAMPERRLOG(err, "register VDEC callback");
    }
  } else {
    OMX_LOGE("mListener is not created yet.");
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}


HRESULT OmxAmpVideoDecoder::unRegisterEvent(AMP_EVENT_CODE event) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mListener) {
    AMP_RPC(err, AMP_VDEC_UnregisterNotify, mAmpHandle,
        AMP_Event_GetServiceID(mListener), event);
    CHECKAMPERRLOG(err, "unregister VDEC notify");

    if (!err) {
      err = AMP_Event_UnregisterCallback(mListener, event, eventHandle);
      CHECKAMPERRLOG(err, "unregister VDEC callback");
    }
  } else {
    OMX_LOGE("mListener is not created yet.");
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

void OmxAmpVideoDecoder::postVideoPresenceNotification(OMX_VIDEO_PRESENCE videoPresence) {
  if (mVideoPresence != videoPresence) {
    mVideoPresence = videoPresence;
    OMX_LOGD("receive the video present notification %d", videoPresence);
    postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoPresentChanged), mVideoPresence, 0);
  }
}

void OmxAmpVideoDecoder::notifySourceVideoInfoChanged() {
  if ((-1 != mSourceId) && (mSourceControl !=NULL)) {
    if ((mVdecInfo.nIsFirstIFrameMg)
        && (0 != mVdecInfo.nMaxWidth)
        && (0 != mVdecInfo.nMaxHeight)) {
      mSourceControl->notifySourceVideoInfo(mSourceId,
          mVdecInfo.nMaxWidth, mVdecInfo.nMaxHeight,
          mVdecInfo.nStereoMode, mVdecInfo.nIsIntlSeq,
          (0 == mVdecInfo.nFrameRate) ? 25000 : mVdecInfo.nFrameRate,
          (0 == mVdecInfo.nSarWidth) ? 1 : mVdecInfo.nSarWidth,
          (0 == mVdecInfo.nSarHeight) ? 1 : mVdecInfo.nSarHeight);
      mSourceControl->applyPQ(mSourceId);
      OMX_LOGD("video info: width:%d height:%d is_intl:%d "
        "framerate*1000:%d sar_width:%d sar_height:%d stereo_mode:%d\n",
        mVdecInfo.nMaxWidth, mVdecInfo.nMaxHeight,
        mVdecInfo.nIsIntlSeq, mVdecInfo.nFrameRate,
        mVdecInfo.nSarWidth, mVdecInfo.nSarHeight,
        mVdecInfo.nStereoMode);
    }
  }
}

void OmxAmpVideoDecoder::AVSettingObserver::OnSettingUpdate(const char *name,
    const AVSettingValue& value) {
  HRESULT result;
  if (-1 == mVdec->mVideoPlaneId) {
    OMX_LOGV("AvSettingObserver is not enabled in non-tunnel mode.");
    return;
  }
  char subkey[marvell::MAX_KEY_LEN];
  int32_t source_id = -1;
  if (!AVSettingsHelper::ParseSourceKey(name, "mrvl.source", &source_id, subkey)) {
    OMX_LOGV("AvSettingObserver ParseSourceKey error, event %s", name);
    return;
  }
  OMX_LOGV("AvSettingObserver source_id %d, subkey %s", source_id, subkey);
  if (source_id == mVdec->mSourceId && strstr(name, marvell::kGfxVideoHoleHidden)) {
    if (value.getBool()) {
      OMX_LOGD("AvSettingObserver hide the video plane.");
      AMP_RPC(result, AMP_DISP_SetPlaneMute, mVdec->mDispHandle,
          mVdec->mVideoPlaneId, 1);
      CHECKAMPERRNOTRETURN(result);
    } else {
      OMX_LOGD("AvSettingObserver show the video plane");
      AMP_RPC(result, AMP_DISP_SetPlaneMute, mVdec->mDispHandle,
          mVdec->mVideoPlaneId, 0);
      CHECKAMPERRNOTRETURN(result);
    }
  } else {
    OMX_LOGV("AvSettingObserver ignore the event.");
  }
}

HRESULT OmxAmpVideoDecoder::eventHandle(HANDLE hListener, AMP_EVENT *pEvent,
    VOID *pUserData) {
  if (!pEvent) {
    OMX_LOGE("pEvent is NULL!");
    return !SUCCESS;
  }
  const AMP_EVENT_CODE eventCode = pEvent->stEventHead.eEventCode;
  OmxAmpVideoDecoder *pComp = static_cast<OmxAmpVideoDecoder *>(pUserData);
  if (AMP_EVENT_API_VDEC_CALLBACK == eventCode) {
    UINT32 *payload = static_cast<UINT32 *>(AMP_EVENT_PAYLOAD_PTR(pEvent));
    HRESULT err = SUCCESS;
    int newDar = OMX_VIDEO_ASPECT_RATIO_UNKNOWN;
    switch (pEvent->stEventHead.uiParam1) {
      case AMP_VDEC_EVENT_RES_CHANGE:
        pComp->mVdecInfo.nMaxWidth = payload[0];
        pComp->mVdecInfo.nMaxHeight = payload[1];
        pComp->mVdecInfo.mMaxNumDisBuf = payload[2];
        OMX_LOGD("Resolution Change: Width %d, Height %d, NeedDisBuf %d, NowDisBuf %d, BufSize %d",
            payload[0], payload[1], payload[2], pComp->mOutputPort->mDefinition.nBufferCountActual,
            pComp->mOutputPort->mDefinition.nBufferSize);
        if (pComp->mOutputPort->isTunneled() == OMX_FALSE) {
          if (pComp->mVdecInfo.mMaxNumDisBuf + 2 >
              pComp->mOutputPort->mDefinition.nBufferCountActual) {
            pComp->unregisterBds(pComp->mOutputPort);
            pComp->mHasRegisterOutBuf = OMX_FALSE;
            pComp->mOutputPort->mDefinition.nBufferCountActual =
                pComp->mVdecInfo.mMaxNumDisBuf + 2;
            pComp->postEvent(OMX_EventPortSettingsChanged, 1, 0);
          } else {
            pComp->mStartPushOutBuf = OMX_TRUE;
          }
        }
#ifdef NOTIFY_RESOLUTION_CHANGE
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoResolutionChange),
            pComp->mVdecInfo.nMaxWidth, pComp->mVdecInfo.nMaxHeight);
#endif
        //for VideoProperties
        OMX_LOGD("notify OMX_EventVideoWidthChanged(%d)", pComp->mVdecInfo.nMaxWidth);
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoWidthChanged),
                  pComp->mVdecInfo.nMaxWidth, 0);

        OMX_LOGD("notify OMX_EventVideoHeightChanged(%d)", pComp->mVdecInfo.nMaxHeight);
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoHeightChanged),
                  pComp->mVdecInfo.nMaxHeight, 0);

        if (pComp->mVdecInfo.nMaxHeight != 0 && pComp->mVdecInfo.nMaxWidth != 0) {
          pComp->mSar= (float)(pComp->mVdecInfo.nMaxWidth) / pComp->mVdecInfo.nMaxHeight;
        }
        //end for VideoProperties
        pComp->notifySourceVideoInfoChanged();
        break;
      case AMP_VDEC_EVENT_FR_CHANGE:
        pComp->mVdecInfo.nFrameRateNum = payload[0];
        pComp->mVdecInfo.nFrameRateDen = payload[1];
        OMX_LOGD("nFrameRateNum: %d, nFrameRateDen: %d",
                 pComp->mVdecInfo.nFrameRateNum, pComp->mVdecInfo.nFrameRateDen);
        if ((0 !=pComp->mVdecInfo.nFrameRateDen) && (0 !=pComp->mVdecInfo.nFrameRateNum)) {
          /*
           * In some cases, nFrameRateNum and nFrameRateDen can be very large
           * (e.g., 20000000/834166)
           * After multiplying 1000, it would overflow for 32-bit.
           * So cast them to OMX_U64 first.
           */
          pComp->mVdecInfo.nFrameRate = (((OMX_U64 )pComp->mVdecInfo.nFrameRateNum) * 1000)
              /((OMX_U64 )pComp->mVdecInfo.nFrameRateDen);

          //for VideoProperties
          OMX_LOGD("notify OMX_EventVideoFramerateChanged(%d)", pComp->mVdecInfo.nFrameRate);
          pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoFramerateChanged),
            pComp->mVdecInfo.nFrameRate, 0);
          //end for VideoProperties

          if (NULL != pComp->mAmpHandle) {
            OMX_LOGD("VDEC get the framerate *1000 = %d from decoder.",
                pComp->mVdecInfo.nFrameRate);
            // Now omx sets framerate(from ffmpegextractor) to vdec in function prepare(),
            // but it isn't accurate to calculate pts if streams can't have pts. To avoid this
            // issue, omx sets the framerate as 0 if VDEC can decode framerate form ES data
            // (it is more accurate than ffmpeg's), then vdec will use the decoder's framerate
            // to handle pts. It is also very useful for variable framerate streams.
            AMP_RPC(err, AMP_VDEC_SetCmdP2, pComp->mAmpHandle, AMP_VDEC_CMD_SET_FRAME_RATE, 0, 0);
            CHECKAMPERRLOG(err, "reset frame rate for vdec.");
          }
        }
        pComp->notifySourceVideoInfoChanged();
        break;
      case AMP_VDEC_EVENT_AR_CHANGE:
        pComp->mVdecInfo.nSarWidth = payload[0];
        pComp->mVdecInfo.nSarHeight = payload[1];
        //for VideoProperties
        OMX_LOGD("notify OMX_EventVideoAspectRatioChanged(%d, %d)", payload[0], payload[1]);

        if (pComp->mVdecInfo.nSarWidth != 0 && pComp->mVdecInfo.nSarHeight != 0) {
          pComp->mPar = (float)(pComp->mVdecInfo.nSarWidth) / pComp->mVdecInfo.nSarHeight;
        } else {
          pComp->mPar = 1.0f;
        }

        pComp->mDar = pComp->mSar * pComp->mPar;

        if (fabs(pComp->mDar - 1.0f) < 0.001f) {
          newDar = OMX_VIDEO_ASPECT_RATIO_1_1;
        } else if (fabs(pComp->mDar - 1.333f) < 0.001f) {
          newDar = OMX_VIDEO_ASPECT_RATIO_4_3;
        } else if (fabs(pComp->mDar - 1.777f) < 0.001f) {
          newDar = OMX_VIDEO_ASPECT_RATIO_16_9;
        } else {
          newDar = OMX_VIDEO_ASPECT_RATIO_UNKNOWN;
        }

        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoAspectRatioChanged),
          newDar, 0);
        //end for VideoProperties
        pComp->notifySourceVideoInfoChanged();
        break;
      case AMP_VDEC_EVENT_1ST_I_DECODED:
        OMX_LOGD("first I-frame decoded");
#ifdef VIDEO_DECODE_ONE_FRAME_MODE
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventIDECODED), 0, 0);
#endif
        pComp->mVdecInfo.nMaxWidth = payload[0];
        pComp->mVdecInfo.nMaxHeight = payload[1];
        pComp->mVdecInfo.nIsIntlSeq = static_cast<OMX_BOOL>(payload[2]);
        pComp->mVdecInfo.nFrameRateNum = payload[3];
        pComp->mVdecInfo.nFrameRateDen = payload[4];
        pComp->mVdecInfo.nSarWidth = payload[5];
        pComp->mVdecInfo.nSarHeight = payload[6];
        pComp->mVdecInfo.nIsFirstIFrameMg = OMX_TRUE;
        if (0 !=pComp->mVdecInfo.nFrameRateDen) {
          OMX_LOGD("nFrameRateNum: %d, nFrameRateDen: %d",
                   pComp->mVdecInfo.nFrameRateNum, pComp->mVdecInfo.nFrameRateDen);
          /*
           * In some cases, nFrameRateNum and nFrameRateDen can be very large
           * (e.g., 20000000/834166)
           * After multiplying 1000, it would overflow for 32-bit.
           * So cast them to OMX_U64 first.
           */
          pComp->mVdecInfo.nFrameRate = (((OMX_U64 )pComp->mVdecInfo.nFrameRateNum) * 1000)
              /(OMX_U64 )(pComp->mVdecInfo.nFrameRateDen);
        }
        pComp->postVideoPresenceNotification(OMX_VIDEO_PRESENCE_TRUE);
        pComp->notifySourceVideoInfoChanged();
        break;
      case AMP_VDEC_EVENT_SCAN_TYPE_CHANGE:
        OMX_LOGD("video scan type change detected, new scan type: %d", payload[0]);
        pComp->mVdecInfo.nIsIntlSeq = static_cast<OMX_BOOL>(payload[0]);
        if (pComp->mVdecInfo.nIsIntlSeq == true) {
          pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoScanTypeChanged),
              OMX_VIDEO_SCAN_TYPE_INTERLACED, 0);
        } else {
          pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideoScanTypeChanged),
              OMX_VIDEO_SCAN_TYPE_PROGRESSIVE, 0);
        }
        pComp->notifySourceVideoInfoChanged();
        break;
      case AMP_VDEC_EVENT_STREAM_DONE:
        pComp->postVideoPresenceNotification(OMX_VIDEO_PRESENCE_FALSE);
        OMX_LOGD("got stream done event from vdec");
        break;
      case AMP_VDEC_EVENT_ONE_FRAME_DECODED:
        pComp->postVideoPresenceNotification(OMX_VIDEO_PRESENCE_TRUE);
        break;
      case AMP_VDEC_EVENT_ERROR_HANDLE:
        OMX_LOGW("serious error in decoder detected");
        pComp->postVideoPresenceNotification(OMX_VIDEO_PRESENCE_FALSE);
        break;
      default:
        return SUCCESS;
    }
  } else if (AMP_VDEC_EVENT_3D_MODE_CHANGE == eventCode) {
    UINT32 *payload = static_cast<UINT32 *>(AMP_EVENT_PAYLOAD_PTR(pEvent));
    FP_INFO *pFPInfo;
    pFPInfo = (FP_INFO*)payload;
    if(pFPInfo != NULL && pFPInfo->valid){
        pComp->mVdecInfo.nStereoMode = mapStereoMode(pFPInfo->convert_mode_3d);
        OMX_LOGD("stereo_mode = %d\n", pComp->mVdecInfo.nStereoMode);
        //for VideoProperties
        OMX_LOGD("notify OMX_EventVideo3DFormatChanged(%d)", pComp->mVdecInfo.nStereoMode);
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventVideo3DFormatChanged),
          pComp->mVdecInfo.nStereoMode, 0);
        //end for VideoProperties
        pComp->notifySourceVideoInfoChanged();
    }
  } else if (AMP_EVENT_API_VIDEO_UNDERFLOW == eventCode) {
    OMX_LOGW("video underflow detected");
    pComp->postVideoPresenceNotification(OMX_VIDEO_PRESENCE_FALSE);
  } else {
    OMX_LOGW("unregistered event type from vdec: %d", eventCode);
  }
  return SUCCESS;
}

}

