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

/**
 * @file BerlinOmxAmpAudioDecoder.cpp
 * @author  csheng@marvell.com
 * @date 2013.01.17
 * @brief  connect AMP ADEC component to decoder audio format : aac, mp3, dolby..
 */

#define LOG_TAG "BerlinOmxAmpAudioDecoder"
#include <BerlinOmxAmpAudioDecoder.h>
#include <amp_crypto_types.h>
extern "C" {
#include <amp_event_listener.h>
}
using namespace android;


namespace berlin {

// AMP port index starts from 0, define a start number to make code more readable.
static const UINT32 kAMPADECPortStartNumber = 0x0;
static const OMX_U32 kMp3DecoderDelay = 529;
static const OMX_U32 stream_in_buf_size = 1 * 1024 * 1024 + 512 * 1024;
static const OMX_U32 stream_in_bd_num = 256;
static const OMX_U32 stream_in_align_size = 0;

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

static inline OMX_S32 ff_mpa_check_header(OMX_U32 header){
  /* header */
  if ((header & 0xffe00000) != 0xffe00000)
    return -1;
  /* layer check */
  if ((header & (3<<17)) == 0)
    return -1;
  /* bit rate */
  if ((header & (0xf<<12)) == 0xf<<12)
   return -1;
  /* frequency */
  if ((header & (3<<10)) == 3<<10)
    return -1;
  return 0;
}


static OMX_BOOL CheckMp3FrameValid(OMX_U8 *frame, OMX_U32 size) {
  if (size < 4) {
    return OMX_FALSE;
  }
  OMX_U32 header = static_cast<OMX_U32>(frame[0]) << 24 |
      static_cast<OMX_U32>(frame[1]) << 16 |
      static_cast<OMX_U32>(frame[2]) << 8 | frame[3];
  OMX_LOGD("mp3 frame header is 0x%x%x%x%x", frame[0], frame[1], frame[2], frame[3]);
  if (ff_mpa_check_header(header) < 0) {
    return OMX_FALSE;
  }
  return OMX_TRUE;
}

/**
 * @brief init Omx AudioDecoder
 */
OmxAmpAudioDecoder::OmxAmpAudioDecoder() {
}

/**
 * @brief init Omx AudioDecoder
 * @param name OMX component name string
 */
OmxAmpAudioDecoder::OmxAmpAudioDecoder(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
}

/**
 * @brief deinit Omx AudioDecoder
 */
OmxAmpAudioDecoder::~OmxAmpAudioDecoder() {
  OMX_LOGD("destroyed");
}

/**
 * @brief init input role
 * @return OMX_ErrorNone if success
 *             OMX_ErrorInvalidComponentName if componaent name is invalid
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char *role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AAC_SECURE,
      OMX_ROLE_AUDIO_DECODER_AAC_SECURE_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AAC_SECURE);
#if PLATFORM_SDK_VERSION >= 19
    mSecure = OMX_TRUE;
#else
    kdMemcpy(mDrm.nUUID, UUID_WIDEVINE, sizeof(mDrm.nUUID));
#endif
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AAC,
      OMX_ROLE_AUDIO_DECODER_AAC_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AAC);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AMRNB,
      OMX_ROLE_AUDIO_DECODER_AMRNB_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AMRNB);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AMRWB,
      OMX_ROLE_AUDIO_DECODER_AMRWB_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AMRWB);
  }
#ifdef OMX_AudioExt_h
  else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_MP1,
      OMX_ROLE_AUDIO_DECODER_MP1_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_MP1);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_MP2,
      OMX_ROLE_AUDIO_DECODER_MP2_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_MP2);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AC3_SECURE,
      OMX_ROLE_AUDIO_DECODER_AC3_SECURE_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AC3_SECURE);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AC3,
      OMX_ROLE_AUDIO_DECODER_AC3_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AC3);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_DTS,
      OMX_ROLE_AUDIO_DECODER_DTS_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_DTS);
  }
#endif
  else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_MP3,
      OMX_ROLE_AUDIO_DECODER_MP3_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_MP3);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_VORBIS,
      OMX_ROLE_AUDIO_DECODER_VORBIS_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_VORBIS);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_G711,
      OMX_ROLE_AUDIO_DECODER_G711_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_G711);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_WMA_SECURE,
      OMX_ROLE_AUDIO_DECODER_WMA_SECURE_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_WMA_SECURE);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_WMA,
      OMX_ROLE_AUDIO_DECODER_WMA_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_WMA);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_RAW,
      OMX_ROLE_AUDIO_DECODER_RAW_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_RAW);
#ifdef ENABLE_EXT_RA
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_RA,
      OMX_ROLE_AUDIO_DECODER_RA_LEN)) {
    addRole(OMX_ROLE_AUDIO_DECODER_RA);
#endif
  } else if (!strncmp(role_name, "audio_decoder", 13)) {
    addRole(OMX_ROLE_AUDIO_DECODER_AAC_SECURE);
    addRole(OMX_ROLE_AUDIO_DECODER_AAC);
    addRole(OMX_ROLE_AUDIO_DECODER_VORBIS);
    addRole(OMX_ROLE_AUDIO_DECODER_G711);
#ifdef OMX_AudioExt_h
    addRole(OMX_ROLE_AUDIO_DECODER_MP1);
    addRole(OMX_ROLE_AUDIO_DECODER_MP2);
    addRole(OMX_ROLE_AUDIO_DECODER_AC3_SECURE);
    addRole(OMX_ROLE_AUDIO_DECODER_AC3);
    addRole(OMX_ROLE_AUDIO_DECODER_DTS);
#endif
    addRole(OMX_ROLE_AUDIO_DECODER_WMA_SECURE);
    addRole(OMX_ROLE_AUDIO_DECODER_WMA);
    addRole(OMX_ROLE_AUDIO_DECODER_MP3);
    addRole(OMX_ROLE_AUDIO_DECODER_RAW);
    addRole(OMX_ROLE_AUDIO_DECODER_AMRNB);
    addRole(OMX_ROLE_AUDIO_DECODER_AMRWB);
#ifdef ENABLE_EXT_RA
    addRole(OMX_ROLE_AUDIO_DECODER_RA);
#endif
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

/**
 * @brief init input port and output port
 * @return OMX_ErrorNone
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::initPort() {
  OMX_LOGD("initPort");
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_AAC_SECURE,
      OMX_ROLE_AUDIO_DECODER_AAC_SECURE_LEN)) {
    mInputPort = new OmxAmpAacPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_AAC,
      OMX_ROLE_AUDIO_DECODER_AAC_LEN)) {
    mInputPort = new OmxAmpAacPort(kAudioPortStartNumber, OMX_DirInput);
  }
#ifdef OMX_AudioExt_h
  else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_MP1,
      OMX_ROLE_AUDIO_DECODER_MP1_LEN)) {
    mInputPort = new OmxAmpMp1Port(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_MP2,
      OMX_ROLE_AUDIO_DECODER_MP2_LEN)) {
    mInputPort = new OmxAmpMp2Port(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_AC3_SECURE,
      OMX_ROLE_AUDIO_DECODER_AC3_SECURE_LEN)) {
    mInputPort = new OmxAmpAc3Port(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_AC3,
      OMX_ROLE_AUDIO_DECODER_AC3_LEN)) {
    mInputPort = new OmxAmpAc3Port(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_DTS,
      OMX_ROLE_AUDIO_DECODER_DTS_LEN)) {
    mInputPort = new OmxAmpDtsPort(kAudioPortStartNumber, OMX_DirInput);
  }
#endif
    else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_MP3,
      OMX_ROLE_AUDIO_DECODER_MP3_LEN)) {
    mInputPort = new OmxAmpMp3Port(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_VORBIS,
      OMX_ROLE_AUDIO_DECODER_VORBIS_LEN)) {
    mInputPort = new OmxAmpVorbisPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_G711,
      OMX_ROLE_AUDIO_DECODER_G711_LEN)) {
    mInputPort = new OmxAmpPcmPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_WMA_SECURE,
      OMX_ROLE_AUDIO_DECODER_WMA_SECURE_LEN)) {
    mInputPort = new OmxAmpWmaPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_WMA,
      OMX_ROLE_AUDIO_DECODER_WMA_LEN)) {
    mInputPort = new OmxAmpWmaPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_AMR,
      OMX_ROLE_AUDIO_DECODER_AMR_LEN)) {
    mInputPort = new OmxAmpAmrPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_RAW,
      OMX_ROLE_AUDIO_DECODER_RAW_LEN)) {
    mInputPort = new OmxAmpPcmPort(kAudioPortStartNumber, OMX_DirInput);
#ifdef ENABLE_EXT_RA
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_RA,
      OMX_ROLE_AUDIO_DECODER_RA_LEN)) {
    mInputPort = new OmxAmpRaPort(kAudioPortStartNumber, OMX_DirInput);
#endif
  } else {
    mInputPort = new OmxAmpAudioPort(kAudioPortStartNumber, OMX_DirInput);
  }
  addPort(mInputPort);
  mOutputPort = new OmxAmpAudioPort(kAudioPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);
  OMX_PARAM_PORTDEFINITIONTYPE def;
  mInputPort->getDefinition(&def);
  def.nBufferCountActual = 4;
  mInputPort->setDefinition(&def);
  return err;
}

void OmxAmpAudioDecoder::initParameter() {
  mEOS = OMX_FALSE;
  mThread = NULL;
  mThreadAttr = NULL;
  mPauseCond = NULL;
  mPauseLock = NULL;
  mBufferLock = NULL;
  mMark.hMarkTargetComponent = NULL;
  mMark.pMarkData = NULL;
  mCodecSpecficDataSent = OMX_FALSE;
  mShouldExit = OMX_FALSE;
  mInputFrameNum = 0;
  mOutputFrameNum = 0;
  mInBDBackNum = 0;
  mOutBDPushNum = 0;
  mStreamPosition = 0;
  mInited = OMX_FALSE;
  mOutputEOS = OMX_FALSE;
  mDelayOutputEOS = OMX_FALSE;
  mPushCount = 0;
  mTimeStampUs = -1;
  mPaused = OMX_FALSE;
  mAudioDrm = OMX_FALSE;
  mDMXHandle = NULL;
  mAudioLength = 0;
  mDrm_type = 0;
  mSchemeIdSend = OMX_FALSE;
  mhInputBuf = NULL;
  mOutputConfigured = OMX_FALSE;
  mOutputConfigChangedComplete = OMX_FALSE;
  mOutputConfigInit = OMX_FALSE;
  mSourceControl = NULL;
  mListener = NULL,
  mSourceId = -1;
  mAudioChannel = -1;
  mPhyDRCMode = -1;
  mPhyChmap = -1;
  mStereoMode = -1;
  mDualMode = -1;
  mPool = NULL;
  mPushedBdNum = 0;
  mReturnedBdNum = 0;
  mCachedhead = NULL;
  mMediaHelper = OMX_FALSE;
  mWMDRM = OMX_FALSE;
  mOutputControl = NULL;
  mObserver = NULL;
}

/**
 * @brief get AMP audio decoder instance mAmpHandle
 * @return AMP API AMP_FACTORY_CreateComponent result
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::getAmpAudioDecoder() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  err = AMP_GetFactory(&mFactory);
  CHECKAMPERRLOG(err, "get AMP factory");
  AMP_RPC(err, AMP_FACTORY_CreateComponent, mFactory, AMP_COMPONENT_ADEC,
      1, &mAmpHandle);
  CHECKAMPERRLOG(err, "create AMP audio decoder");
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief destroy AMP decoder
 * @return return AMP API AMP_ADEC_Destory result
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::destroyAmpAudioDecoder() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mAudioDrm == OMX_TRUE && mDMXHandle) {
    AMP_RPC(err, AMP_DMX_Destroy, mDMXHandle);
    CHECKAMPERRLOGNOTRETURN(err, "destroy DMX handler");
    AMP_FACTORY_Release(mDMXHandle);
    mDMXHandle = NULL;
  }
  if (mAmpHandle) {
    AMP_RPC(err, AMP_ADEC_Destroy, mAmpHandle);
    CHECKAMPERRLOGNOTRETURN(err, "destroy AMP handler");
    AMP_FACTORY_Release(mAmpHandle);
    mAmpHandle = NULL;
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief judge if support adec or not
 * @return OMX_TRUE : support
 *         OMX_FALSE : not support
 */
OMX_BOOL OmxAmpAudioDecoder::isAudioParamSupport(
    OMX_AUDIO_PARAM_PORTFORMATTYPE* audio_param) {
  OMX_BOOL isSupport = OMX_FALSE;
  for (OMX_U32 audio_index = 0; audio_index < NELM(kAudioFormatSupport);
      audio_index ++) {
    if (audio_param->eEncoding == static_cast<OMX_AUDIO_CODINGTYPE>
          (kAudioFormatSupport[audio_index].eCompressionFormat)) {
      isSupport = OMX_TRUE;
      break;
    }
  }
  return isSupport;
}

OMX_ERRORTYPE OmxAmpAudioDecoder::getAmpAudioInfo(int adecAudioType, AMP_ADEC_AUD_INFO* audinfo) {
  if (!mAmpHandle) {
    OMX_LOGE("No adec component created");
    return OMX_ErrorUndefined;
  }

  HRESULT errCode = SUCCESS;
  switch (adecAudioType) {
    case AMP_MPG_AUDIO :
      audinfo->_d = AMP_ADEC_PARAIDX_MPG;
      AMP_RPC(errCode, AMP_ADEC_GetInfoWithFmt, mAmpHandle, AMP_MPG_AUDIO, audinfo);
      CHECKAMPERRLOGNOTRETURN(errCode, "AMP_ADEC_GetInfoWithFmt(AMP_MPG_AUDIO)");
      break;
    case AMP_HE_AAC :
      audinfo->_d = AMP_ADEC_PARAIDX_AAC;
      AMP_RPC(errCode, AMP_ADEC_GetInfoWithFmt, mAmpHandle, AMP_HE_AAC, audinfo);
      CHECKAMPERRLOGNOTRETURN(errCode, "AMP_ADEC_GetInfoWithFmt(AMP_HE_AAC)");
      break;
    case AMP_M4A_LATM:
      audinfo->_d = AMP_ADEC_PARAIDX_M4ALATM;
      AMP_RPC(errCode, AMP_ADEC_GetInfoWithFmt, mAmpHandle, AMP_M4A_LATM, audinfo);
      CHECKAMPERRLOGNOTRETURN(errCode, "AMP_ADEC_GetInfoWithFmt(AMP_M4A_LATM)");
      break;
    case AMP_MS11_DDC :
      audinfo->_d = AMP_ADEC_PARAIDX_MS11_DDC;
      AMP_RPC(errCode, AMP_ADEC_GetInfoWithFmt, mAmpHandle, AMP_MS11_DDC, audinfo);
      CHECKAMPERRLOGNOTRETURN(errCode, "AMP_ADEC_GetInfoWithFmt(AMP_MS11_DDC)");
      break;
    default:
      OMX_LOGW("Unsupport yet, adecAudioType=%d", adecAudioType);
      break;
  }
  return OMX_ErrorNone;
}

/**
 * @brief get parameter
 * @return return OMX_ErrorNone or others
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::getParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamAudioPortFormat: {
      OMX_AUDIO_PARAM_PORTFORMATTYPE *audio_param =
          reinterpret_cast<OMX_AUDIO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(audio_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(audio_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (mInputPort->getPortIndex() == audio_param->nPortIndex) {
        if (audio_param->nIndex >= NELM(kAudioFormatSupport)) {
          err = OMX_ErrorNoMore;
          break;
        }
        const AudioFormatType *a_fmt = &kAudioFormatSupport[audio_param->nIndex];
        audio_param->eEncoding = a_fmt->eCompressionFormat;
      } else {
        if (audio_param->nIndex >= 1) {
          err = OMX_ErrorNoMore;
          break;
        }
        OMX_AUDIO_PORTDEFINITIONTYPE &a_def = mOutputPort->getAudioDefinition();
        audio_param->eEncoding = a_def.eEncoding;
      }
      break;
    }
    case OMX_IndexParamAudioAac: {
      OMX_AUDIO_PARAM_AACPROFILETYPE* aacParams =
          reinterpret_cast<OMX_AUDIO_PARAM_AACPROFILETYPE*>(params);
      err = CheckOmxHeader(aacParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(aacParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
        *aacParams = ((OmxAmpAacPort*)(port))->getCodecParam();
      }
      break;
    }
#ifdef OMX_IndexExt_h
    case OMX_IndexParamAudioMp1: {
      OMX_AUDIO_PARAM_MP1TYPE* mp1_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP1TYPE*>(params);
      err = CheckOmxHeader(mp1_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(mp1_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP1) {
        *mp1_param = static_cast<OmxAmpMp1Port*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioMp2: {
      OMX_AUDIO_PARAM_MP2TYPE* mp2_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP2TYPE*>(params);
      err = CheckOmxHeader(mp2_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(mp2_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP2) {
        *mp2_param = static_cast<OmxAmpMp2Port*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioAc3: {
      OMX_AUDIO_PARAM_AC3TYPE *ac3Params =
          reinterpret_cast<OMX_AUDIO_PARAM_AC3TYPE *>(params);
      err = CheckOmxHeader(ac3Params);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(ac3Params->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAC3) {
        *ac3Params = static_cast<OmxAmpAc3Port*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioDts: {
      OMX_AUDIO_PARAM_DTSTYPE *dtsParams =
          reinterpret_cast<OMX_AUDIO_PARAM_DTSTYPE *>(params);

      err = CheckOmxHeader(dtsParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(dtsParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingDTS) {
        *dtsParams = static_cast<OmxAmpDtsPort*>(port)->getCodecParam();
      }
      break;
    }

    case OMX_IndexExtAudioGetProfile: {
      OMX_U32* ret = reinterpret_cast<OMX_U32*>(params);
      OMX_U32 profile, level;
      getAudioProfileLevel(profile, level);
      *ret = profile;
      break;

    }
    case OMX_IndexExtAudioGetLevel: {
      OMX_U32* ret = reinterpret_cast<OMX_U32*>(params);
      OMX_U32 profile, level;
      getAudioProfileLevel(profile, level);
      *ret = level;
      break;
    }
#endif
    case OMX_IndexParamAudioMp3: {
      OMX_AUDIO_PARAM_MP3TYPE* codec_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP3TYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3) {
        *codec_param = static_cast<OmxAmpMp3Port*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioVorbis: {
      OMX_AUDIO_PARAM_VORBISTYPE *vorbisParams =
          reinterpret_cast<OMX_AUDIO_PARAM_VORBISTYPE *>(params);

      err = CheckOmxHeader(vorbisParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(vorbisParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingVORBIS) {
        *vorbisParams = static_cast<OmxAmpVorbisPort*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioPcm: {
      OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
          reinterpret_cast<OMX_AUDIO_PARAM_PCMMODETYPE *>(params);
      /* As issue https://code.google.com/p/google-tv/issues/detail?id=6050#c8
           * OpenMax 1.1 and 1.2 difference, the ->nSize is differenct
           * OMX_AUDIO_MAXCHANNELS is not the same : 16 vs 36
           * so do not check header size here.
           */
      /* err = CheckOmxHeader(pcmParams);
          if (OMX_ErrorNone != err) {
            OMX_LOGD("line %d\n", __LINE__);
            break;
          }
          */
      OmxPortImpl* port = getPort(pcmParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      OMX_LOGD("pcmparam size %d struc size %d", pcmParams->nSize,
          sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      if (pcmParams->nPortIndex == kAudioPortStartNumber) {
        if (port->getDomain() == OMX_PortDomainAudio &&
            (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
            port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
          if (pcmParams->nSize == sizeof(OMX_AUDIO_PARAM_PCMMODETYPE)) {
            *pcmParams = static_cast<OmxAmpPcmPort*>(port)->getCodecParam();
          } else {
            OMX_U32 nSize = pcmParams->nSize;
            kdMemcpy(pcmParams,
                &(static_cast<OmxAmpPcmPort*>(port)->getCodecParam()),
                pcmParams->nSize);
            pcmParams->nSize = nSize;
          }
        }
      } else if (pcmParams->nPortIndex == (kAudioPortStartNumber + 1)) {
        if (port->getDomain() == OMX_PortDomainAudio &&
            (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
            port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
          if (pcmParams->nSize == sizeof(OMX_AUDIO_PARAM_PCMMODETYPE)) {
            *pcmParams = static_cast<OmxAmpAudioPort*>(port)->getPcmOutParam();
          } else {
            OMX_U32 nSize = pcmParams->nSize;
            kdMemcpy(pcmParams,
                &(static_cast<OmxAmpAudioPort*>(port)->getPcmOutParam()),
                pcmParams->nSize);
            pcmParams->nSize = nSize;
          }
        }
      } else {
        OMX_LOGE("Error pcm port index %d", pcmParams->nPortIndex);
      }
      break;
    }
    case OMX_IndexParamAudioWma: {
      OMX_AUDIO_PARAM_WMATYPE *wmaParams =
          reinterpret_cast<OMX_AUDIO_PARAM_WMATYPE *>(params);

      err = CheckOmxHeader(wmaParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(wmaParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingWMA) {
        *wmaParams = static_cast<OmxAmpWmaPort*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioAmr: {
      OMX_AUDIO_PARAM_AMRTYPE *amrParams =
          reinterpret_cast<OMX_AUDIO_PARAM_AMRTYPE *>(params);

      err = CheckOmxHeader(amrParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(amrParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAMR) {
        *amrParams = static_cast<OmxAmpAmrPort*>(port)->getCodecParam();
      }
      break;
    }
#ifdef ENABLE_EXT_RA
    case OMX_IndexParamAudioRa: {
      OMX_AUDIO_PARAM_EXT_RATYPE *raParams =
          reinterpret_cast<OMX_AUDIO_PARAM_EXT_RATYPE *>(params);

      err = CheckOmxHeader(raParams);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("Bad parameter!");
        break;
      }
      OmxPortImpl* port = getPort(raParams->nPortIndex);
      if (NULL == port) {
        OMX_LOGE("Bad port index!");
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingRA) {
        *raParams = static_cast<OmxAmpRaPort*>(port)->getCodecParam();
      }
      break;
    }
#endif
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_U32 OmxAmpAudioDecoder::getAudioProfileLevel(OMX_U32& profile, OMX_U32& level) {
  OMX_AUDIO_CODINGTYPE coding_type = mInputPort->getAudioDefinition().eEncoding;
  OMX_S32 amp_type = getAmpDecType(coding_type);

  AMP_ADEC_AUD_INFO audinfo;
  OMX_LOGD("audio info codec type %d, amp %d", coding_type, amp_type);

  getAmpAudioInfo(amp_type, &audinfo);

  OMX_U32 pos;

  switch (coding_type){
    case OMX_AUDIO_CodingAAC:
      OMX_LOGD("aac audio info, profile=%d", audinfo._u.stAacInfo.uiProfile);
      pos = audinfo._u.stAacInfo.uiProfile - 1;
      if (pos == OMX_AUDIO_AACObjectHE) {
        /*HE-AAC V1 Profile (2)*/
        profile = 2;
      } else if (pos == OMX_AUDIO_AACObjectHE_PS) {
        profile = 3; /*HE-AAC V2 Profile (3)*/
      } else {
        profile = 1; /*default: AAC profile (1)*/
      }
      level = 0;
      break;

    case OMX_AUDIO_CodingMP1:
      profile = 1;
      level = 0;
      break;
    case OMX_AUDIO_CodingMP2:
      profile = 2;
      level = 0;
      break;
    case OMX_AUDIO_CodingMP3:
      profile = 3;
      level = 0;
      break;
    case OMX_AUDIO_CodingAC3:
      OMX_LOGD("dolby audio info, ac3=%d", audinfo._u.stMs11DdcInfo.uiBsType);
      if (audinfo._u.stMs11DdcInfo.uiBsType == AMP_DD_AC3) {
        profile = 2;/*Dolby AC3 (2)*/
      } else if (audinfo._u.stMs11DdcInfo.uiBsType == AMP_DD_PLUS) {
        profile = 3;/*Dolby Enhanced AC3  (3)*/
      } else {
        profile = 1;/*default: maybe DOLBY A  (1)*/
      }
      level = 0;
      break;
    case OMX_AUDIO_CodingDTS:
      profile = 1;
      level = 0;
      break;
    default:
      break;
  }

  return 0;
}

/**
 * @brief set parameter
 * @return return OMX_ErrorNone or others
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::setParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamAudioPortFormat: {
      OMX_AUDIO_PARAM_PORTFORMATTYPE* audio_param =
          reinterpret_cast<OMX_AUDIO_PARAM_PORTFORMATTYPE*>(params);
      err = CheckOmxHeader(audio_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(audio_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio) {
        if (isAudioParamSupport(audio_param)) {
          port->setAudioParam(audio_param);
          port->updateDomainDefinition();
        } else {
          // @todo find a proper error NO.
          err = OMX_ErrorNotImplemented;
        }
      }
      break;
    }
    case OMX_IndexParamAudioAac: {
      OMX_AUDIO_PARAM_AACPROFILETYPE* aacParams =
          reinterpret_cast<OMX_AUDIO_PARAM_AACPROFILETYPE*>(params);
      err = CheckOmxHeader(aacParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(aacParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
        static_cast<OmxAmpAacPort*>(port)->setCodecParam(aacParams);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            aacParams->nSampleRate, aacParams->nChannels);
      }
      break;
    }
#ifdef OMX_IndexExt_h
    case OMX_IndexParamAudioMp1: {
      OMX_AUDIO_PARAM_MP1TYPE* codec_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP1TYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP1) {
        static_cast<OmxAmpMp1Port*>(port)->setCodecParam(codec_param);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            codec_param->nSampleRate, codec_param->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioMp2: {
      OMX_AUDIO_PARAM_MP2TYPE* codec_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP2TYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP2) {
        static_cast<OmxAmpMp2Port*>(port)->setCodecParam(codec_param);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            codec_param->nSampleRate, codec_param->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioMp3: {
      OMX_AUDIO_PARAM_MP3TYPE* codec_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP3TYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3) {
        static_cast<OmxAmpMp3Port*>(port)->setCodecParam(codec_param);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            codec_param->nSampleRate, codec_param->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioAc3: {
      OMX_AUDIO_PARAM_AC3TYPE* ac3Params =
          reinterpret_cast<OMX_AUDIO_PARAM_AC3TYPE*>(params);
      err = CheckOmxHeader(ac3Params);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(ac3Params->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAC3) {
        static_cast<OmxAmpAc3Port*>(port)->setCodecParam(ac3Params);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            ac3Params->nSampleRate, ac3Params->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioDts: {
      OMX_AUDIO_PARAM_DTSTYPE* dtsParams =
          reinterpret_cast<OMX_AUDIO_PARAM_DTSTYPE*>(params);
      err = CheckOmxHeader(dtsParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(dtsParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingDTS) {
        static_cast<OmxAmpDtsPort*>(port)->setCodecParam(dtsParams);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            dtsParams->nSampleRate, dtsParams->nChannels);
      }
      break;
    }

#endif
    case OMX_IndexParamAudioVorbis: {
      OMX_AUDIO_PARAM_VORBISTYPE* vorbisParams =
          reinterpret_cast<OMX_AUDIO_PARAM_VORBISTYPE*>(params);
      err = CheckOmxHeader(vorbisParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(vorbisParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingVORBIS) {
        static_cast<OmxAmpVorbisPort*>(port)->setCodecParam(vorbisParams);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            vorbisParams->nSampleRate, vorbisParams->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioPcm: {
      OMX_AUDIO_PARAM_PCMMODETYPE* pcmParams =
          reinterpret_cast<OMX_AUDIO_PARAM_PCMMODETYPE*>(params);
      OMX_LOGD("port index %d", pcmParams->nPortIndex);
      /* As issue https://code.google.com/p/google-tv/issues/detail?id=6050#c8
           * OpenMax 1.1 and 1.2 difference, the ->nSize is differenct
           * OMX_AUDIO_MAXCHANNELS is not the same : 16 vs 36
           * so do not check header size here.
           */
      /* err = CheckOmxHeader(pcmParams);
          if (OMX_ErrorNone != err) {
            OMX_LOGD("line %d\n", __LINE__);
            break;
          }
          */
      OmxPortImpl* port = getPort(pcmParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
        if (pcmParams->nPortIndex == kAudioPortStartNumber) {
          static_cast<OmxAmpPcmPort*>(port)->setCodecParam(pcmParams);
          (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
              pcmParams->nSamplingRate);
        } else if (pcmParams->nPortIndex == (kAudioPortStartNumber + 1)) {
          static_cast<OmxAmpAudioPort*>(port)->setPcmOutParam(pcmParams);
        } else {
          OMX_LOGE("Error pcm port index %d", pcmParams->nPortIndex);
        }
      }
      break;
    }
    case OMX_IndexParamAudioWma: {
      OMX_AUDIO_PARAM_WMATYPE* wmaParams =
          reinterpret_cast<OMX_AUDIO_PARAM_WMATYPE*>(params);
      err = CheckOmxHeader(wmaParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(wmaParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingWMA) {
        static_cast<OmxAmpWmaPort*>(port)->setCodecParam(wmaParams);
        (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
            wmaParams->nSamplingRate, wmaParams->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioAmr: {
      OMX_AUDIO_PARAM_AMRTYPE* amrParams =
          reinterpret_cast<OMX_AUDIO_PARAM_AMRTYPE*>(params);
      err = CheckOmxHeader(amrParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(amrParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAMR) {
        static_cast<OmxAmpAmrPort*>(port)->setCodecParam(amrParams);
        // TODO:: Amr has no sample rate property now
        // (static_cast<OmxAmpAudioPort *>(mOutputPort))->SetOutputParameter(
        //    amrParams->nSamplingRate);
      }
      break;
    }
#ifdef ENABLE_EXT_RA
    case OMX_IndexParamAudioRa: {
      OMX_AUDIO_PARAM_EXT_RATYPE* raParams =
          reinterpret_cast<OMX_AUDIO_PARAM_EXT_RATYPE *>(params);
      err = CheckOmxHeader(raParams);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("Bad parameter!");
        break;
      }
      OmxPortImpl* port = getPort(raParams->nPortIndex);
      if (NULL == port) {
        OMX_LOGE("Bad port index!");
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingRA) {
        static_cast<OmxAmpRaPort*>(port)->setCodecParam(raParams);
      }
      break;
    }
#endif

#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_IndexParamMediaContainer: {
      // override OmxComponentImpl
      OMX_MEDIACONTAINER_INFOTYPE *container_type =
          reinterpret_cast<OMX_MEDIACONTAINER_INFOTYPE *>(params);
      mContainerType = *container_type;
      OMX_LOGD("format type = %d", mContainerType.eFmtType);
      break;
    }
#endif

    default:
      return OmxComponentImpl::setParameter(index, params);
  }
  return err;
}

/**
* @brief set wma parameter
*/
OMX_ERRORTYPE OmxAmpAudioDecoder::setWMAParameter(
    OMX_U8 *extra_data, OMX_U32 extra_size) {
  HRESULT err = SUCCESS;
  OmxPortImpl* port = NULL;
  AMP_ADEC_PARAS mParas;
  memset(&mParas, 0, sizeof(mParas));
  OMX_AUDIO_PARAM_WMATYPE wmaParams;
  memset(&wmaParams, 0x0, sizeof(OMX_AUDIO_PARAM_WMATYPE));
  port = getPort(kAudioPortStartNumber);
  if (NULL == port) {
    err = OMX_ErrorBadPortIndex;
    return static_cast<OMX_ERRORTYPE>(err);
  }
  if (port->getDomain() == OMX_PortDomainAudio &&
      port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingWMA) {
    wmaParams = static_cast<OmxAmpWmaPort*>(port)->getCodecParam();
  }
  mParas._d = AMP_ADEC_PARAIDX_WMA;
  mParas._u.WMA.uiBitRate = wmaParams.nBitRate;
  mParas._u.WMA.unBitDepth = wmaParams.nBitsPerSample;
  mParas._u.WMA.nBlockAlign = wmaParams.nBlockAlign;
  mParas._u.WMA.unChanNr = wmaParams.nChannels;
  mParas._u.WMA.uiSampleRate = wmaParams.nSamplingRate;
  switch(wmaParams.eFormat) {
    case OMX_AUDIO_WMAFormat9:
      mParas._u.WMA.unFormatTag = 0x0161;
      break;
    case OMX_AUDIO_WMAFormat9_Professional:
      mParas._u.WMA.unFormatTag = 0x0162;
      break;
    case OMX_AUDIO_WMAFormat9_Lossless:
      mParas._u.WMA.unFormatTag = 0x0163;
      break;
    default:
      OMX_LOGE("unsupported format %d\n", wmaParams.eFormat);
      return static_cast<OMX_ERRORTYPE>(ERR_ERRPARAM);
      break;
  }
  mParas._u.WMA.iAvgBytesPerSec = wmaParams.nBitRate / 8;
  mParas._u.WMA.iDecoderFlags = 0;
  // mParas._u.WMA.iPacketSize = ;
  // mParas._u.WMA.nPad = ;
  mParas._u.WMA.nPeakAmplitudeRef = 0;
  mParas._u.WMA.nPeakAmplitudeTarget = 0;
  mParas._u.WMA.nRmsAmplitudeRef = 0;
  mParas._u.WMA.nRmsAmplitudeTarget = 0;
  // mParas._u.WMA.ucExtraData = ;
  // mParas._u.WMA.uiChanMask = ;
  // mParas._u.WMA.uiInThresh = ;
  // mParas._u.WMA.unChanMode = wmaParams->;
  // mParas._u.WMA.unLfeMode = wmaParams->;
  static OMX_U32 kWMAExtradataSize = 32;
  mParas._u.WMA.nCbSize = kWMAExtradataSize;
  OMX_U8 data[kWMAExtradataSize];
  kdMemset(data, 0, kWMAExtradataSize);
  if (extra_data == NULL) {
    mParas._u.WMA.nPad = 1;
  } else {
    if (extra_size >= 4) {
      data[0] = extra_data[0];
      data[1] = extra_data[1];
      data[2] = extra_data[2];
      data[3] = extra_data[3];
    }
  }
  data[4] = static_cast<OMX_U8>((wmaParams.nEncodeOptions) & 0xff);
  data[5] = static_cast<OMX_U8>(((wmaParams.nEncodeOptions) >> 8) & 0xff);
  data[6] = static_cast<OMX_U8>((wmaParams.nSuperBlockAlign) & 0xff);
  data[7] = static_cast<OMX_U8>(((wmaParams.nSuperBlockAlign) >> 8) & 0xff);
  data[8] = static_cast<OMX_U8>(((wmaParams.nSuperBlockAlign) >> 16) & 0xff);
  data[9] = static_cast<OMX_U8>(((wmaParams.nSuperBlockAlign) >> 24) & 0xff);
  data[10] = static_cast<OMX_U8>((wmaParams.nAdvancedEncodeOpt) & 0xff);
  data[11] = static_cast<OMX_U8>(((wmaParams.nAdvancedEncodeOpt) >> 8) & 0xff);
  data[12] = static_cast<OMX_U8>(((wmaParams.nAdvancedEncodeOpt) >> 16) & 0xff);
  data[13] = static_cast<OMX_U8>(((wmaParams.nAdvancedEncodeOpt) >> 24) & 0xff);
  data[14] = static_cast<OMX_U8>((wmaParams.nAdvancedEncodeOpt2) & 0xff);
  data[15] = static_cast<OMX_U8>(((wmaParams.nAdvancedEncodeOpt2) >> 8) & 0xff);
  data[16] = static_cast<OMX_U8>(((wmaParams.nAdvancedEncodeOpt2) >> 16) & 0xff);
  data[17] = static_cast<OMX_U8>(((wmaParams.nAdvancedEncodeOpt2) >> 24) & 0xff);
  kdMemcpy(mParas._u.WMA.ucExtraData, data, kWMAExtradataSize);
  AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
      AMP_ADEC_PARAIDX_WMA, &mParas);
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief set the pointed audio parameter
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::setADECParameter(
    OMX_S32 adec_type) {
  HRESULT err = SUCCESS;
  AMP_ADEC_PARAIDX para_index = 0;
  OmxPortImpl* port = NULL;
  AMP_ADEC_PARAS mParas;
  OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
  memset(&mParas, 0, sizeof(mParas));
  // A map from channel number to channel mode in AMP.
  const int channelmode[9] = {0, 1, 2, 3, 3, 7, 7, 8, 8};

  switch (adec_type) {
    case AMP_HE_AAC:
      mParas._d = AMP_ADEC_PARAIDX_AAC;
      mParas._u.AAC.ucLayer = 0;
      mParas._u.AAC.ucProfileType = 1;  // _LC;
      mParas._u.AAC.ucStreamFmt = 0;  // AAC_SF_MP2ADTS;
      mParas._u.AAC.ucVersion = 1;  // MPEG2
      mParas._u.AAC.uiBitRate = 6000;
      mParas._u.AAC.uiChanMask = 0x2;  // L R
      mParas._u.AAC.uiInThresh = (768 * 6);
      mParas._u.AAC.uiSampleRate = 4;  // 44100
      mParas._u.AAC.unBitDepth = 8;
      mParas._u.AAC.unChanMode = 2;  // IPP_STEREO;
      mParas._u.AAC.unChanNr = 2;
      mParas._u.AAC.unLfeMode = 0;
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_AAC, &mParas);
      break;
    case AMP_M4A_LATM:
      mParas._d = AMP_ADEC_PARAIDX_M4ALATM;
      mParas._u.M4ALATM.m_Layer = AMP_M4ALATM_LAYER_LOAS;
      mParas._u.M4ALATM.m_Format = AMP_M4ALATM_LOASFMT_ASS;
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_M4ALATM, &mParas);
      break;
    case AMP_G711A:
    case AMP_G711U:
      port = getPort(kAudioPortStartNumber);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        OMX_LOGE("failed to get port");
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
        pcmParams = static_cast<OmxAmpPcmPort*>(port)->getCodecParam();
        OMX_LOGD("get OMX_AUDIO_PARAM_PCMMODETYPE");
      } else {
        OMX_LOGE("no OMX_AUDIO_PARAM_PCMMODETYPE found");
        break;
      }
      mParas._d = AMP_ADEC_PARAIDX_G711;
      mParas._u.G711.uiSampleRate = pcmParams.nSamplingRate;
      mParas._u.G711.unChanNr = pcmParams.nChannels;
      mParas._u.G711.unBitDepth = pcmParams.nBitPerSample;
      mParas._u.G711.uiInThresh = 0x4000;
      OMX_LOGD("sample rate = %d, channel count = %d, bit depth = %d",
          mParas._u.G711.uiSampleRate, mParas._u.G711.unChanNr, mParas._u.G711.unBitDepth);
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_G711, &mParas);
      break;

    case AMP_RAW_PCM:
      port = getPort(kAudioPortStartNumber);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        OMX_LOGE("failed to get port");
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM) {
        pcmParams = static_cast<OmxAmpPcmPort*>(port)->getCodecParam();
        OMX_LOGD("get OMX_AUDIO_PARAM_PCMMODETYPE");
      } else {
        OMX_LOGE("no OMX_AUDIO_PARAM_PCMMODETYPE found");
        break;
      }
      mParas._d = AMP_ADEC_PARAIDX_RAWPCM;
      mParas._u.PCM.uiPcmType = AMP_AUDIO_PCM_16BIT;
      if ((8 == pcmParams.nBitPerSample) &&
          (OMX_NumericalDataUnsigned == pcmParams.eNumData)) {
        mParas._u.PCM.uiPcmType = AMP_AUDIO_PCMBITS8_UNSIGNED;
      } else if ((8 == pcmParams.nBitPerSample) &&
          (OMX_NumericalDataSigned == pcmParams.eNumData)) {
        mParas._u.PCM.uiPcmType = AMP_AUDIO_PCMBITS8_SINGED;
      } else if ((16 == pcmParams.nBitPerSample) &&
          (OMX_NumericalDataUnsigned == pcmParams.eNumData)) {
        mParas._u.PCM.uiPcmType = AMP_AUDIO_PCMBITS16_UNSIGNED;
      } else if ((24 == pcmParams.nBitPerSample) &&
          (OMX_NumericalDataUnsigned == pcmParams.eNumData)) {
        mParas._u.PCM.uiPcmType = AMP_AUDIO_PCMBITS24_UNSIGNED;
      } else if ((24 == pcmParams.nBitPerSample) &&
          (OMX_NumericalDataSigned == pcmParams.eNumData)) {
        mParas._u.PCM.uiPcmType = AMP_AUDIO_PCMBITS24_SINGED;
      }
      // When PCM.uiChanMask is set, PCM.unChanMode will be ingnored in AMP.
      mParas._u.PCM.unChanMode = channelmode[pcmParams.nChannels];
      switch (pcmParams.nChannels) {
        case 1:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_CNTR;
          break;
        case 2:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          break;
        case 3:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          mParas._u.PCM.uiChanMap[2] = AMP_AUDIO_CHMASK_CNTR;
          break;
        case 4:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          mParas._u.PCM.uiChanMap[2] = AMP_AUDIO_CHMASK_CNTR;
          mParas._u.PCM.uiChanMap[3] = AMP_AUDIO_CHMASK_LFE;
          break;
        case 5:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          mParas._u.PCM.uiChanMap[2] = AMP_AUDIO_CHMASK_CNTR;
          mParas._u.PCM.uiChanMap[3] = AMP_AUDIO_CHMASK_SRRD_LEFT;
          mParas._u.PCM.uiChanMap[4] = AMP_AUDIO_CHMASK_SRRD_RGHT;
          break;
        case 6:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          mParas._u.PCM.uiChanMap[2] = AMP_AUDIO_CHMASK_CNTR;
          mParas._u.PCM.uiChanMap[3] = AMP_AUDIO_CHMASK_LFE;
          mParas._u.PCM.uiChanMap[4] = AMP_AUDIO_CHMASK_SRRD_LEFT;
          mParas._u.PCM.uiChanMap[5] = AMP_AUDIO_CHMASK_SRRD_RGHT;
          break;
        case 7:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          mParas._u.PCM.uiChanMap[2] = AMP_AUDIO_CHMASK_CNTR;
          mParas._u.PCM.uiChanMap[3] = AMP_AUDIO_CHMASK_SRRD_LEFT;
          mParas._u.PCM.uiChanMap[4] = AMP_AUDIO_CHMASK_SRRD_RGHT;
          mParas._u.PCM.uiChanMap[5] = AMP_AUDIO_CHMASK_LEFT_REAR;
          mParas._u.PCM.uiChanMap[6] = AMP_AUDIO_CHMASK_RGHT_REAR;
          break;
        case 8:
          mParas._u.PCM.uiChanMap[0] = AMP_AUDIO_CHMASK_LEFT;
          mParas._u.PCM.uiChanMap[1] = AMP_AUDIO_CHMASK_RGHT;
          mParas._u.PCM.uiChanMap[2] = AMP_AUDIO_CHMASK_CNTR;
          mParas._u.PCM.uiChanMap[3] = AMP_AUDIO_CHMASK_LFE;
          mParas._u.PCM.uiChanMap[4] = AMP_AUDIO_CHMASK_SRRD_LEFT;
          mParas._u.PCM.uiChanMap[5] = AMP_AUDIO_CHMASK_SRRD_RGHT;
          mParas._u.PCM.uiChanMap[6] = AMP_AUDIO_CHMASK_LEFT_REAR;
          mParas._u.PCM.uiChanMap[7] = AMP_AUDIO_CHMASK_RGHT_REAR;
          break;
        default:
          break;
      }
      for (int idx = 0; idx < 8; idx++) {
        mParas._u.PCM.uiChanMask += mParas._u.PCM.uiChanMap[idx];
      }
      mParas._u.PCM.cLittleEndian = 0;
      if (OMX_EndianBig == pcmParams.eEndian) {
        mParas._u.PCM.cLittleEndian = 1;
      }
      mParas._u.PCM.uiInThresh = 0x4000;
      mParas._u.PCM.uiSampleRate = pcmParams.nSamplingRate;
      mParas._u.PCM.unChanNr = pcmParams.nChannels;
      mParas._u.PCM.unBitDepth = pcmParams.nBitPerSample;
      OMX_LOGD("sample rate = %d, channel count = %d, bit depth = %d",
          mParas._u.PCM.uiSampleRate, mParas._u.PCM.unChanNr, mParas._u.PCM.unBitDepth);
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_RAWPCM, &mParas);
      break;

    case AMP_WMA:
      err = setWMAParameter();
      break;

    case AMP_MS11_DDC:
      mParas._d = AMP_ADEC_PARAIDX_MS11_DDC;
      mParas._u.MS11_DDC.nDualInput = FALSE;
      mParas._u.MS11_DDC.nDualDecode = FALSE;
      mParas._u.MS11_DDC.nOutputMain = TRUE;
      mParas._u.MS11_DDC.nOutputSec = FALSE;
      mParas._u.MS11_DDC.iSubStreamSelect = -1;
      mParas._u.MS11_DDC.iUserBalanceAdj = 0;
      mParas._u.MS11_DDC.iDrcMode[0] = (-1 != mPhyDRCMode) ? mPhyDRCMode :
          AMP_AUDIO_COMPMODE_ON;
      mParas._u.MS11_DDC.iDrcLow[0] = 0x40000000L;
      mParas._u.MS11_DDC.iDrcHigh[0] = 0x40000000L;
      mParas._u.MS11_DDC.iDrcLow[1] = 0x40000000L;
      mParas._u.MS11_DDC.iDrcHigh[1] = 0x40000000L;
      mParas._u.MS11_DDC.iStereoMode = (-1 != mStereoMode) ? mStereoMode :
          AMP_AUDIO_STEREO_STEREO;
      mParas._u.MS11_DDC.iDualMode = (-1 != mDualMode) ? mDualMode :
          AMP_AUDIO_DUALMODE_STEREO;
      mParas._u.MS11_DDC.iKMode = AMP_AUDIO_KARAOKE_BOTH_VOCAL;
      mParas._u.MS11_DDC.iChanMode = ((AMP_AUDIO_CHMAP_2_0_0 == mPhyChmap) ?
          AMP_AUDIO_CHMAP_2_0_0 : AMP_AUDIO_CHMAP_3_2_0);
      mParas._u.MS11_DDC.iLfeMode = ((AMP_AUDIO_CHMAP_2_0_0 == mPhyChmap) ?
          AMP_AUDIO_LFE_OFF : AMP_AUDIO_LFE_ON);
      OMX_LOGD("mPhyChmap =%0x, chanmode = %0x, unLfeMode = %0x, "
          "mPhyDRCMode = %0x, iDRCMode = %0x, mStereoMode = %0x, "
          "iStereoMode = %0x, mDualMode = %0x, iDualMode = %0x",
          mPhyChmap, mParas._u.MS11_DDC.iChanMode, mParas._u.MS11_DDC.iLfeMode,
          mPhyDRCMode, mParas._u.MS11_DDC.iDrcMode[0], mStereoMode,
          mParas._u.MS11_DDC.iStereoMode, mDualMode,
          mParas._u.MS11_DDC.iDualMode);
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_MS11_DDC, &mParas);
      break;

#ifdef ENABLE_EXT_RA
    case AMP_REAL_AUD8:
      OMX_AUDIO_PARAM_EXT_RATYPE raParams;
      // kAudioPortStartNumber need to get
      port = getPort(kAudioPortStartNumber);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingRA) {
        raParams = static_cast<OmxAmpRaPort*>(port)->getCodecParam();
      }
      memset(&mParas, 0, sizeof(mParas));
      mParas._d = AMP_ADEC_PARAIDX_RA8;
      mParas._u.RA8.uiCodec4CC = 0x636f6f6b;
      mParas._u.RA8.uiSampleRate = raParams.nSamplingRate;
      mParas._u.RA8.uiActualRate = raParams.nSamplingRate;
      mParas._u.RA8.unBitDepth = raParams.nBitsPerFrame;
      mParas._u.RA8.unChanNr = raParams.nChannels;
      mParas._u.RA8.unAudioQuality = 100;
      mParas._u.RA8.unFlavorIndex = raParams.nFlavorIndex;
      mParas._u.RA8.uiBitsPerFrame = raParams.nSamplePerFrame;
      mParas._u.RA8.uiGranularity = raParams.nGranularity;
      mParas._u.RA8.uiOpaqueDataSize = raParams.nOpaqueDataSize;
      for (int idx = 0; idx < raParams.nOpaqueDataSize; idx++) {
        mParas._u.RA8.ucOpaqueData[idx] = raParams.nOpaqueData[idx];
      }
      OMX_LOGD("SampleRate %d, BitDepth %d, ChanNr %d, BitsPerFrame %d",
          mParas._u.RA8.uiSampleRate, mParas._u.RA8.unBitDepth,
          mParas._u.RA8.unChanNr, mParas._u.RA8.uiBitsPerFrame);
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_RA8, &mParas);

      AMP_ADEC_PARAS newParas;
      memset(&newParas, 0, sizeof(newParas));
      AMP_RPC(err, AMP_ADEC_GetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_RA8, &newParas);
      OMX_LOGD("Get RA param uiCodec4CC=0x%x, uiSampleRate=%d, "
          "uiActualRate=%d, unBitDepth=%d, unChanNr=%d, "
          "unAudioQuality=%d, unFlavorIndex=%d, "
          "uiBitsPerFrame=%d, uiGranularity=%d",
          newParas._u.RA8.uiCodec4CC,
          newParas._u.RA8.uiSampleRate,
          newParas._u.RA8.uiActualRate,
          newParas._u.RA8.unBitDepth,
          newParas._u.RA8.unChanNr,
          newParas._u.RA8.unAudioQuality,
          newParas._u.RA8.unFlavorIndex,
          newParas._u.RA8.uiBitsPerFrame,
          newParas._u.RA8.uiGranularity);
      for (int idx = 0; idx < newParas._u.RA8.uiOpaqueDataSize; idx++) {
        OMX_LOGD("Get RA param, ucOpaqueData[%d] = %d",
            idx, newParas._u.RA8.ucOpaqueData[idx]);
      }
      break;
#endif
    case AMP_DTS_HD:
      mParas._d = AMP_ADEC_PARAIDX_DTS;
      // Just for distinguish container DTS-Wav from DTS-Others
      if (mContainerType.eFmtType == OMX_FORMAT_WAVE) {
        mParas._u.DTS.cDtsWav = OMX_TRUE;
      }
      OMX_LOGD("set AMP_ADEC_PARAIDX_DTS, cDtsWav->%d", mParas._u.DTS.cDtsWav);
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle, AMP_ADEC_PARAIDX_DTS, &mParas);
      CHECKAMPERRLOGNOTRETURN(err, "set AMP_ADEC_PARAIDX_DTS");
      break;
  }
  CHECKAMPERRLOG(err, "set parameter to audio decoder");
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief get AMP decoder type : MEDIA_AES_AAC, MEDIA_AES_MP3...
 * @return return AMP decoder type
 */
OMX_S32 OmxAmpAudioDecoder::getAmpDecType(
    OMX_AUDIO_CODINGTYPE type) {
  OmxPortImpl* port;
  OMX_S32 amp_type = -1;
  switch (type) {
    case OMX_AUDIO_CodingAAC:
      amp_type = AMP_HE_AAC;
      OMX_AUDIO_PARAM_AACPROFILETYPE aacParams;
      port = getPort(kAudioPortStartNumber);
      if (port != NULL && port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
        aacParams = static_cast<OmxAmpAacPort*>(port)->getCodecParam();
        if (aacParams.eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4LATM) {
          amp_type = AMP_M4A_LATM;
        }
      }
      break;
#ifdef OMX_IndexExt_h
    case OMX_AUDIO_CodingMP1:
    case OMX_AUDIO_CodingMP2:
      amp_type = AMP_MPG_AUDIO;
      break;
    case OMX_AUDIO_CodingAC3:
      amp_type = AMP_MS11_DDC;
      break;
    case OMX_AUDIO_CodingDTS:
      amp_type = AMP_DTS_HD;
      break;
#endif
    case OMX_AUDIO_CodingMP3:
      amp_type = AMP_MP3;
      break;
    case OMX_AUDIO_CodingVORBIS:
      amp_type = AMP_VORBIS;
      break;
    case OMX_AUDIO_CodingPCM:
    case OMX_AUDIO_CodingG711:
      OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
      amp_type = AMP_RAW_PCM;
      // kAudioPortStartNumber need to get
      port = getPort(kAudioPortStartNumber);
      if (port != NULL && port->getDomain() == OMX_PortDomainAudio &&
          (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
        pcmParams = static_cast<OmxAmpPcmPort*>(port)->getCodecParam();
        if (pcmParams.ePCMMode == OMX_AUDIO_PCMModeLinear) {
          amp_type = AMP_RAW_PCM;
        } else if (pcmParams.ePCMMode == OMX_AUDIO_PCMModeALaw) {
          amp_type = AMP_G711A;
        } else if (pcmParams.ePCMMode == OMX_AUDIO_PCMModeMULaw) {
          amp_type = AMP_G711U;
        }
#ifdef ENABLE_LPCM_BLURAY
        else if (pcmParams.ePCMMode == OMX_AUDIO_PCMModeBluray) {
          amp_type = AMP_LPCM_BD;
        }
#endif
      }
      break;
    case OMX_AUDIO_CodingWMA:
      amp_type = AMP_WMA;
      break;
    case OMX_AUDIO_CodingAMR: {
      char *role_name = mName + kCompNamePrefixLength;
      if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AMRNB,
          OMX_ROLE_AUDIO_DECODER_AMRNB_LEN)) {
        amp_type = AMP_AMRNB;
      } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AMRWB,
          OMX_ROLE_AUDIO_DECODER_AMRWB_LEN)) {
        amp_type = AMP_AMRWB;
      }
      break;
    }
    case OMX_AUDIO_CodingRA: {
      amp_type = AMP_REAL_AUD8;
      break;
    }
    default :
      OMX_LOGE("amp audio codec %d not supported", type);
  }
  return amp_type;
}

OMX_ERRORTYPE OmxAmpAudioDecoder::configDmxPort() {
  HRESULT err = SUCCESS;
  AMP_COMPONENT_CONFIG mConfig;
  static const char kUuidNoDrm[sizeof(mDrm.nUUID)] = { 0 };
  static const char kUuidWidevine[] = UUID_WIDEVINE;
  static const char kUuidPlayReady[] = UUID_PLAYREADY;
  static const char kUuidMarLin[] = UUID_MARLIN;
  if (kdMemcmp(mDrm.nUUID, kUuidNoDrm, sizeof(mDrm.nUUID))) {
    if (kdMemcmp(mDrm.nUUID, kUuidWidevine, sizeof(mDrm.nUUID)) == 0) {
      OMX_LOGD("Widevine");
      mAudioDrm = OMX_TRUE;
      mDrm_type = AMP_DRMSCHEME_WIDEVINE;
    } else if (kdMemcmp(mDrm.nUUID, kUuidPlayReady, sizeof(mDrm.nUUID)) == 0) {
#ifndef DISABLE_PLAYREADY
      OMX_LOGD("WMDRM detected");
#else
      OMX_LOGD("PlayReady is not supported.");
      return OMX_ErrorUndefined;
#endif
      mAudioDrm = OMX_TRUE;
      mDrm_type = AMP_DRMSCHEME_PLAYREADY;
    } else if (kdMemcmp(mDrm.nUUID, kUuidMarLin, sizeof(mDrm.nUUID)) == 0) {
      OMX_LOGD("MarLin");
      mAudioDrm = OMX_TRUE;
      mDrm_type = AMP_DRMSCHEME_MARLIN;
    } else {
      OMX_LOGE("Unsupported DRM: "
          "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          mDrm.nUUID[0], mDrm.nUUID[1], mDrm.nUUID[2], mDrm.nUUID[3],
          mDrm.nUUID[4], mDrm.nUUID[5], mDrm.nUUID[6], mDrm.nUUID[7],
          mDrm.nUUID[8], mDrm.nUUID[9], mDrm.nUUID[10], mDrm.nUUID[11],
          mDrm.nUUID[12], mDrm.nUUID[13], mDrm.nUUID[14], mDrm.nUUID[15]);
      return OMX_ErrorUndefined;
    }
    // create DMX
    if (mAudioDrm == OMX_TRUE) {
      OMX_LOGD("Create DMX component");
      AMP_RPC(err, AMP_FACTORY_CreateComponent, mFactory, AMP_COMPONENT_DMX,
          0, &mDMXHandle);
      CHECKAMPERRLOG(err, "create DMX component");
      kdMemset(&mConfig, 0 , sizeof(mConfig));
      mConfig._d = AMP_COMPONENT_DMX;
      mConfig._u.pDMX.mode = AMP_SECURE_TUNNEL;
      // mConfig._u.pDMX.uiFlag = 0;  /* streamin mode*/
      mConfig._u.pDMX.uiInputPortNum = 1;
      mConfig._u.pDMX.uiOutputPortNum = 1;
      AMP_RPC(err, AMP_DMX_Open, mDMXHandle, &mConfig);
      CHECKAMPERRLOG(err, "open DMX handler");
    }
  }

  if (mAudioDrm == OMX_TRUE) {
    OMX_LOGD("Add DMX input port...");
    AMP_DMX_INPUT_CFG mInputCfg;

    err = AMP_SHM_Allocate(AMP_SHM_DYNAMIC,
        DMX_ES_BUF_SIZE + DMX_ES_BUF_PAD_SIZE,
        32,
        &mhInputBuf);
    CHECKAMPERRLOG(err, "allocate share memory for DRM");

    mInputCfg.InputType = AMP_DMX_INPUT_ES;
    mInputCfg.hInBuf = mhInputBuf;
    mInputCfg.uiBufSize = DMX_ES_BUF_SIZE;
    mInputCfg.uiBufPaddingSize = DMX_ES_BUF_PAD_SIZE;

    if (mDrm_type == AMP_DRMSCHEME_PLAYREADY
        || mDrm_type == AMP_DRMSCHEME_MARLIN) {
      mInputCfg.CyptCfg.SchemeType = AMP_DMX_AES_128_CTR_NO_NO;
    } else if (mDrm_type == AMP_DRMSCHEME_WIDEVINE) {
      mInputCfg.CyptCfg.SchemeType = AMP_DMX_AES_128_CBC_CTS_CLR;
    }

    AMP_RPC(err, AMP_DMX_AddInput, mDMXHandle, &mInputCfg, &mInputObj);
    CHECKAMPERRLOG(err, "add input to DMX handler");

    static_cast<OmxAmpAudioPort *>(mInputPort)->configMemInfo(mhInputBuf);

    OMX_LOGD("Add DMX output port...");
    AMP_DMX_CH_CFG ChCfg;
    kdMemset(&ChCfg, 0 , sizeof(ChCfg));
    ChCfg.eType = AMP_DMX_OUTPUT_AES;
    ChCfg.Tag = 0;

    mChnlObj.uiPortIdx = 0xFFFFFFFF;
    AMP_RPC(err, AMP_DMX_AddChannel, mDMXHandle, &ChCfg, &mChnlObj);
    CHECKAMPERRLOG(err, "add channel to DMX handler");

    AMP_DMX_CH_PROP Prop;
    kdMemset(&Prop, 0 , sizeof(Prop));
    Prop.uPropMask = AMP_DMX_CH_PROP_EVENTMASK_MASK;
    Prop.Property.EventMask = 0x10;
    AMP_RPC(err, AMP_DMX_ChannelPropSet, mDMXHandle, &mChnlObj, &Prop);
    CHECKAMPERRLOG(err, "set property to DMX handler");

    Prop.uPropMask = AMP_DMX_CH_PROP_EVENTMASK_MASK;
    Prop.Property.EventMask = 0;
    AMP_RPC(err, AMP_DMX_ChannelPropGet, mDMXHandle, &mChnlObj, &Prop);
    CHECKAMPERRLOG(err, "get property of DMX handler ");
    OMX_LOGD("Prop.Property.EventMask = 0x%x", Prop.Property.EventMask);

    AMP_RPC(err, AMP_DMX_ChannelControl, mDMXHandle, &mChnlObj, AMP_DMX_CH_CTRL_ON, 0);
    OMX_LOGD("DMX output port index %d", mChnlObj.uiPortIdx);
    CHECKAMPERRLOG(err, "control channel of DMX handler");
  }
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief prepare, init amp decoder, connect ADEC component, register callback
 * @return AMP ADEC API result
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::prepare() {
  OMX_LOG_FUNCTION_ENTER;
  initParameter();

  // mediahelper init
  if (mediainfo_buildup()) {
    mMediaHelper = OMX_TRUE;
    OMX_LOGD("Media helper build success");
  } else {
    OMX_LOGE("Media helper build fail");
  }
  // remove the dumped data
  if (mMediaHelper == OMX_TRUE) {
    mediainfo_remove_dumped_data(MEDIA_AUDIO);
  }

  HRESULT err = SUCCESS;
  AMP_COMPONENT_CONFIG mConfig;
  mConfig._u.pADEC.uiInPortNum = 1;
  mConfig._u.pADEC.uiOutPortNum = 1;
  // set aren passthru
  if (mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingDTS ||
      mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAC3) {
    if (mOutputPort->isTunneled()) {
      OmxComponentImpl *aren = static_cast<OmxComponentImpl *>(
          static_cast<OMX_COMPONENTTYPE *>(mOutputPort->getTunnelComponent())->pComponentPrivate);
      if (NULL != aren && NULL != strstr(aren->mName, "audio_renderer")) {
        OMX_LOGD("set aren passthru mode");
        static_cast<OmxAmpAudioRenderer *>(aren)->setPassThruMode();
        // set adec output port num to 3: hdmi, pcm, spdif
        mConfig._u.pADEC.uiOutPortNum = 3;
      } else {
        OMX_LOGD("set aren passthru mode fail %d", __LINE__);
      }
    } else {
      OMX_LOGD("set aren passthru mode fail %d", __LINE__);
    }
  }
  // Get audio decoder handle mAmpHandle
  err = getAmpAudioDecoder();
  CHECKAMPERRLOG(err, "get AMP audio decoder");
  OMX_S32 acodec_type = getAmpDecType(
      mInputPort->getAudioDefinition().eEncoding);
  if (acodec_type < 0) {
    OMX_LOGE("can not find the AMP adec type.");
    return OMX_ErrorUndefined;
  }

  initAQ(acodec_type);

  setADECParameter(acodec_type);
  mConfig._d = AMP_COMPONENT_ADEC;
  if (mOutputPort->isTunneled()) {
    mConfig._u.pADEC.mode = AMP_SECURE_TUNNEL;
  } else {
    mConfig._u.pADEC.mode = AMP_NON_TUNNEL;
  }
  mConfig._u.pADEC.eAdecFmt = acodec_type;

#ifdef OMX_IVCommonExt_h
  if (!kdMemcmp(mDrm.nUUID, UUID_WMDRM, sizeof(mDrm.nUUID))) {
#ifndef DISABLE_PLAYREADY
    OMX_LOGD("PlayReady");
#else
    OMX_LOGD("WMDRM is not supported.");
    return OMX_ErrorUndefined;
#endif
    mWMDRM = OMX_TRUE;
  } else {
    configDmxPort();
    CHECKAMPERRLOG(err, "");
  }
#endif

  if (mAudioDrm == OMX_FALSE && mSecure == OMX_TRUE) {
    OMX_LOGD("Enable Secure No Tvp Playback.");
    static_cast<OmxAmpAudioPort *>(mInputPort)->mOmxDisableTvp = OMX_TRUE;
  }

  // use stream in mode for drm stream
  if (mAudioDrm == OMX_TRUE) {
    mConfig._u.pADEC.ucFrameIn = AMP_ADEC_STEAM;
  } else if (mOutputPort->isTunneled() && (
      mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingWMA
      || mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAMR
#ifdef OMX_AudioExt_h
      || mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingDTS
#endif
      )) {
    // workaround for some files one buffer not one frame
    mConfig._u.pADEC.ucFrameIn = AMP_ADEC_STEAM;
  } else {
    mConfig._u.pADEC.ucFrameIn = AMP_ADEC_FRAME;
  }

  // aac latm need stream in mode
  if (mOutputPort->isTunneled() &&
      mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
    OmxPortImpl* port;
    OMX_AUDIO_PARAM_AACPROFILETYPE aacParams;
    port = getPort(kAudioPortStartNumber);
    if (port != NULL) {
      aacParams = static_cast<OmxAmpAacPort*>(port)->getCodecParam();
      if (aacParams.eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4LATM) {
        mConfig._u.pADEC.ucFrameIn = AMP_ADEC_STEAM;
      }
    }
  }

  if (mOutputPort->isTunneled()) {
#if 1
    if (mAudioDrm != OMX_TRUE) {
      OMX_U32 ret;
      if (mWMDRM == OMX_TRUE) {
        // we should use non-secure input buffers before amp adec support wma in TZ.
        ret = AMP_CBUFCreate(stream_in_buf_size, 0, stream_in_bd_num,
            stream_in_align_size, true, &mPool, true, 64 * 1024, AMP_SHM_STATIC, false);
      } else {
        ret = AMP_CBUFCreate(stream_in_buf_size, 0, stream_in_bd_num,
            stream_in_align_size, true, &mPool, true, 64 * 1024, AMP_SHM_STATIC, true);
      }
      if (AMPCBUF_SUCCESS != ret) {
        OMX_LOGW("ADEC Cbuf Create Error %u.", ret);
        // create cbuf fail, use omx bd
        OMX_PARAM_PORTDEFINITIONTYPE def;
        mInputPort->getDefinition(&def);
        def.nBufferCountActual = 128;
        mInputPort->setDefinition(&def);
      }
    } else {
      OMX_PARAM_PORTDEFINITIONTYPE def;
      mInputPort->getDefinition(&def);
      def.nBufferCountActual = 64;
      mInputPort->setDefinition(&def);
    }
#else
    OMX_PARAM_PORTDEFINITIONTYPE def;
    mInputPort->getDefinition(&def);
    def.nBufferCountActual = 128;
    mInputPort->setDefinition(&def);
#endif
  } else {
    if (mAudioDrm == OMX_TRUE) {
      mInputPort->setBufferCount(64);
    }
  }
  AMP_RPC(err, AMP_ADEC_Open, mAmpHandle, &mConfig);
  CHECKAMPERRLOG(err, "open AMP audio decoder handler");

  if (mAudioDrm == OMX_TRUE) {
    err = AMP_ConnectApp(mDMXHandle, AMP_PORT_INPUT,
        0, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect AudioDecoder input port with DMX");
    // connect DMX and ADEC
    err = AMP_ConnectComp(mDMXHandle, mChnlObj.uiPortIdx,
        mAmpHandle, kAMPADECPortStartNumber);
    CHECKAMPERRLOG(err, "connect DMX with ADEC");
  } else {
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_INPUT,
        0, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect AudioDecoder input port with ADEC");
  }

  if (!mOutputPort->isTunneled()) {
    OMX_LOGD("non-tunnel mode, connect output port");
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_OUTPUT,
        0, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect AudioDecoder output port with ADEC");
  }

  // Register a callback so that it can receive event - from ADEC.
  mListener = AMP_Event_CreateListener(AMP_EVENT_TYPE_MAX, 0);
  if (mListener) {
    err = registerEvent(AMP_EVENT_ADEC_CALLBACK_STRMINFO);
    CHECKAMPERRLOG(err, "RegisterEvent with ADEC");
  }

  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(SUCCESS);
}

/**
 * @brief release, close amp decoder, disconnect component
 * @return AMP ADEC API result
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::release() {
  OMX_LOG_FUNCTION_ENTER;
  // deinit mediahelper
  if (mMediaHelper == OMX_TRUE) {
    mediainfo_teardown();
  }
  if (mPool) {
    if (AMPCBUF_SUCCESS != AMP_CBUFDestroy(mPool)) {
      OMX_LOGE("Cbuf destroy pool error.");
    }
  }

  HRESULT err = SUCCESS;
  if (mListener) {
    err = unRegisterEvent(AMP_EVENT_ADEC_CALLBACK_STRMINFO);
    CHECKAMPERRLOGNOTRETURN(err, "nRegisterEvent with ADEC");
    CHECKAMPERRNOTRETURN(AMP_Event_DestroyListener(mListener));
    mListener = NULL;
  }

  if (mAudioDrm == OMX_TRUE) {
    err = AMP_DisconnectApp(mDMXHandle, AMP_PORT_INPUT,
        0, pushBufferDone);
    CHECKAMPERRLOGNOTRETURN(err, "disceonnect AudioDecoder input port with DMX");
  } else {
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_INPUT,
        0, pushBufferDone);
    CHECKAMPERRLOGNOTRETURN(err, "disceonnect AudioDecoder input port with ADEC");
  }
  if (!mOutputPort->isTunneled()) {
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_OUTPUT,
        0, pushBufferDone);
    CHECKAMPERRLOGNOTRETURN(err, "disceonnect AudioDecoder output port with ADEC");
  } else {
#ifdef OMX_CORE_EXT
    OMX_COMPONENTTYPE *aren =
        static_cast<OMX_COMPONENTTYPE *>(mOutputPort->getTunnelComponent());
    OMX_PTR type = kdMalloc(4 + 1);
    if (aren && type) {
      kdMemset(type, 0x0, 5);
      kdMemcpy(type, "adec", 4);
      OMX_SendCommand(aren,
          static_cast<OMX_COMMANDTYPE>(OMX_CommandDisconnectPort), 0, type);
      kdFree(type);
    }
#endif
  }
  if (mAudioDrm == OMX_TRUE) {
     //Clear channel
    AMP_RPC(err, AMP_DMX_ChannelControl, mDMXHandle, &mChnlObj, AMP_DMX_CH_CTRL_OFF, 0);
    CHECKAMPERRLOGNOTRETURN(err, "clear DMX channel");

    AMP_RPC(err, AMP_DMX_RmChannel, mDMXHandle, &mChnlObj);
    CHECKAMPERRLOGNOTRETURN(err, "delete DMX channel");

    AMP_RPC(err, AMP_DMX_RmInput, mDMXHandle, &mInputObj);
    CHECKAMPERRLOGNOTRETURN(err, "delete DMX input");

    AMP_RPC(err, AMP_DMX_Close, mDMXHandle);
    CHECKAMPERRLOGNOTRETURN(err, "close DMX handler");

    AMP_SHM_Release(mhInputBuf);
  }
  AMP_RPC(err, AMP_ADEC_Close, mAmpHandle);
  CHECKAMPERRLOGNOTRETURN(err, "close ADEC");

  err = destroyAmpAudioDecoder();
  CHECKAMPERRLOGNOTRETURN(err, "destroy ADEC");

  while (!mCodecSpeficData.empty()) {
    vector<CodecSpeficDataType>::iterator it = mCodecSpeficData.begin();
    if (it->data != NULL) {
      kdFree(it->data);
    }
    mCodecSpeficData.erase(it);
  }

  if (mOutputControl != NULL) {
    mOutputControl->removeObserver(mObserver);
  }
  if ((mSourceControl != NULL) && (-1 != mSourceId)) {
    mSourceControl->exitSource(mSourceId);
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief preroll
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::preroll() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

/**
 * @brief pause AMP ADEC
 * @return setAmpState result
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::pause() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mPaused = OMX_TRUE;
  mShouldExit = OMX_TRUE;
  err = setAmpState(AMP_PAUSED);
  CHECKAMPERRLOG(err, "set audio decoder to pause state");
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

/**
 * @brief resume AMP ADEC, set AMP state to AMP_EXECUTING
 * @return setAmpState result
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::resume() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  err = setAmpState(AMP_EXECUTING);
  CHECKAMPERRLOG(err, "set audio decoder to executing state");

  if (mAudioDrm == OMX_TRUE) {
    AMP_RPC(err, AMP_DMX_ChannelControl, mDMXHandle, &mChnlObj, AMP_DMX_CH_CTRL_ON, 0);
    CHECKAMPERRLOG(err, "enable DMX channel");
  }
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
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief start AMP ADEC, set AMP state to AMP_EXECUTING, create decode thread
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::start() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  err = registerBds(static_cast<OmxAmpAudioPort *>(mInputPort));
  CHECKAMPERRLOG(err, "register BD in input port");

  if (!mOutputPort->isTunneled()) {
    err = registerBds(static_cast<OmxAmpAudioPort *>(mOutputPort));
    CHECKAMPERRLOG(err, "register BD in output port");
  }

  err = setAmpState(AMP_EXECUTING);
  CHECKAMPERRLOG(err, "Sset audio decoder intput executing state");

  OMX_LOGD("Create decoding thread");
  mPauseLock = kdThreadMutexCreate(KD_NULL);
  mPauseCond = kdThreadCondCreate(KD_NULL);
  mBufferLock = kdThreadMutexCreate(KD_NULL);
  mThread = kdThreadCreate(mThreadAttr, threadEntry,
      reinterpret_cast<void *>(this));
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

/**
 * @brief stop AMP ADEC, set AMP state to AMP_IDLE
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::stop() {
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
    // @todo need correct the value
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
  if (mCachedhead) {
    returnBuffer(mInputPort, mCachedhead);
    mCachedhead = NULL;
  }
  err = setAmpState(AMP_IDLE);
  CHECKAMPERRLOG(err, "set audio decoder intpu IDEL state");

  err = clearAmpPort();
  CHECKAMPERRLOG(err, "clear AMP port");

  // Waiting for input/output buffers back
  OMX_U32 wait_count = 100;
  while ((mInputFrameNum > mInBDBackNum || mOutBDPushNum > mOutputFrameNum)
      && wait_count > 0) {
    usleep(5000);
    wait_count--;
  }
  OMX_LOGD("inputframe %d inbackbd %d outframe %d outpushbd %d wait %d\n",
      mInputFrameNum, mInBDBackNum, mOutputFrameNum, mOutBDPushNum, wait_count);
  if (mPool) {
    wait_count = 150;
    while (mPushedBdNum > mReturnedBdNum && wait_count > 0) {
      usleep(5000);
      wait_count --;
    }
    if (!wait_count) {
      OMX_LOGE("There are %d pushed BD not returned.",
          mPushedBdNum - mReturnedBdNum);
    }
  }
  returnCachedBuffers(mInputPort);
  returnCachedBuffers(mOutputPort);
  err = unregisterBds(static_cast<OmxAmpAudioPort *>(mInputPort));
  CHECKAMPERRLOG(err, "unregister BDs at input port");

  if (!mOutputPort->isTunneled()) {
    err = unregisterBds(static_cast<OmxAmpAudioPort *>(mOutputPort));
    CHECKAMPERRLOG(err, "unregister BDs at output port");
  }
  mShouldExit = OMX_FALSE;
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

/**
 * @brief flush AMP ADEC
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::flush() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 wait_count = 150;
  kdThreadMutexLock(mBufferLock);
  if (!mOutputPort->isTunneled()) {
    // As it would call flushPort() later and operate port->isEmpty(),
    // And pushBufferLoop() will operate port->isEmpty() in another thread,
    // so make sure port->isEmpty() true here to avoid return buffer two times
    OMX_BUFFERHEADERTYPE *in_head = NULL, *out_head = NULL;
    while(!mInputPort->isEmpty()) {
      in_head = mInputPort->popBuffer();
      if (in_head != NULL) {
        OMX_LOGD("return in_head %p when when flushing", in_head);
        returnBuffer(mInputPort, in_head);
        in_head = NULL;
      }
    }
    while(!mOutputPort->isEmpty()) {
      out_head = mOutputPort->popBuffer();
      if (out_head != NULL) {
        OMX_LOGD("return out_head %p when when flushing", out_head);
        returnBuffer(mOutputPort, out_head);
        out_head = NULL;
      }
    }
  }
  if (NULL != mCachedhead) {
    OMX_LOGD("return the cached buffer header.");
    returnBuffer(mInputPort, mCachedhead);
    mCachedhead = NULL;
  }

  err = setAmpState(AMP_IDLE);
  CHECKOMXERRNOTRETURN(err);
  err = clearAmpPort();
  CHECKAMPERRNOTRETURN(err);
  // clear dumped es data and pcm data
  if (CheckMediaHelperEnable(mMediaHelper, CLEAR_DUMP) == OMX_TRUE) {
    mediainfo_remove_dumped_data(MEDIA_AUDIO);
  }
  wait_count = 150;
  while ((mInputFrameNum > mInBDBackNum || mOutBDPushNum > mOutputFrameNum)
      && wait_count > 0) {
    usleep(5000);
    wait_count--;
  }
  OMX_LOGD("inputframe %d inbackbd %d outframe %d outpushbd %d wait %d\n",
      mInputFrameNum, mInBDBackNum, mOutputFrameNum, mOutBDPushNum, wait_count);
  if (NULL != mPool) {
    wait_count = 200;
    while (mPushedBdNum > mReturnedBdNum && wait_count > 0) {
      usleep(5000);
      wait_count--;
    }
    if (!wait_count) {
      OMX_LOGE("There are %d pushed Bd not returned.", mPushedBdNum - mReturnedBdNum);
    }
    if (AMPCBUF_SUCCESS != AMP_CBUFReset(mPool)) {
      OMX_LOGE("Cbuf AMP_CBUFReSet error.");
    }
    mPushedBdNum = 0;
    mReturnedBdNum = 0;
  }
  returnCachedBuffers(mInputPort);
  returnCachedBuffers(mOutputPort);
  // set pts to -1 for update later
  mTimeStampUs = -1;
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
  mDelayOutputEOS = OMX_FALSE;
  mPushCount = 0;
  mAudioLength = 0;
  mInputFrameNum = 0;
  mOutputFrameNum = 0;
  mInBDBackNum = 0;
  mOutBDPushNum = 0;
  static_cast<OmxAmpAudioPort *>(mInputPort)->mPushedOffset = 0;
  kdThreadMutexUnlock(mBufferLock);
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

/**
 * @brief clear amp ports
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::clearAmpPort() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mAudioDrm == OMX_TRUE) {
    AMP_RPC(err, AMP_DMX_ClearPortBuf, mDMXHandle, AMP_PORT_INPUT, mInputObj.uiPortIdx);
    CHECKAMPERRLOG(err, "clear DMX input port buffer");

    AMP_RPC(err, AMP_DMX_ClearPortBuf, mDMXHandle, AMP_PORT_OUTPUT, mChnlObj.uiPortIdx);
    CHECKAMPERRLOG(err, "clear DMX output port buffer");

    AMP_RPC(err, AMP_DMX_ChannelControl, mDMXHandle, &mChnlObj, AMP_DMX_CH_CTRL_OFF, 0);
    CHECKAMPERRLOG(err, "turn off DMX channel");
  } else {
    // AMP_RPC(err, AMP_ADEC_ClearPortBuf, mAmpHandle, AMP_PORT_INPUT, 0);
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief set AMP state
 * @param state AMP target state
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::setAmpState(AMP_STATE state) {
  OMX_LOGD("setAmpState %s", AmpState2String(state));
  HRESULT err = SUCCESS;
  if (mAudioDrm == OMX_TRUE) {
    AMP_RPC(err, AMP_DMX_SetState, mDMXHandle, state);
    CHECKAMPERRLOG(err, "set DMX state");
  }
  AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, state);
  CHECKAMPERRLOG(err, "set ADEC state");

  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief get AMP state
 * @param state store the AMP current state
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::getAmpState(AMP_STATE *state) {
  HRESULT err = SUCCESS;
  if (mAudioDrm == OMX_TRUE) {
    AMP_RPC(err, AMP_DMX_GetState, mDMXHandle, state);
    CHECKAMPERRLOG(err, "get DMX state");
  }
  AMP_RPC(err, AMP_ADEC_GetState, mAmpHandle, state);
  CHECKAMPERRLOG(err, "get ADEC state");

  OMX_LOGD("getAmpState is %s", AmpState2String(*state));
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief push AMP BD
 * @param port port type, input or output
 * @param portindex port index
 * @param bd AMP BD
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::pushAmpBd(AMP_PORT_IO port,
    UINT32 portindex, AMP_BD_ST *bd) {
  if (CheckMediaHelperEnable(mMediaHelper, PUSH_AMPBD) == OMX_TRUE) {
    OMX_LOGD("pushAmpBd, port_IO %d, bd 0x%x, bd.bdid 0x%x, bd.allocva 0x%x",
        port, bd, bd->uiBDId, bd->uiAllocVA);
  }
  HRESULT err = SUCCESS;
  if (mAudioDrm == OMX_TRUE && port == AMP_PORT_INPUT) {
    AMP_RPC(err, AMP_DMX_PushBD, mDMXHandle, port, portindex, bd);
    CHECKAMPERRLOG(err, "push BD to DMX");
  } else {
    AMP_RPC(err, AMP_ADEC_PushBD, mAmpHandle, port, portindex, bd);
    CHECKAMPERRLOG(err, "push BD to ADEC");
  }
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief register AMP BD
 * @param port port structure point
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::registerBds(OmxAmpAudioPort *port) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  UINT32 uiPortIdx = 0;
  AMP_BD_ST *bd;
  if (port->isInput()) {
    for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
      AmpBuffer *amp_buffer = port->getAmpBuffer(i);
      if (NULL != amp_buffer) {
        if (mAudioDrm == OMX_FALSE) {
          amp_buffer->addMemInfoTag();
          amp_buffer->addPtsTag();
          AMP_RPC(err, AMP_ADEC_RegisterBD, mAmpHandle, AMP_PORT_INPUT,
              uiPortIdx, amp_buffer->getBD());
          CHECKAMPERRLOG(err, "register BD to ADEC at input port");
        }
      }
    }
  } else {
    for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
      AmpBuffer *amp_buffer = port->getAmpBuffer(i);
      if (NULL != amp_buffer) {
        // amp_buffer->addMemInfoTag();
        amp_buffer->addAudioFrameInfoTag();
        amp_buffer->addAVSPtsTag();
        AMP_RPC(err, AMP_ADEC_RegisterBD, mAmpHandle, AMP_PORT_OUTPUT,
            uiPortIdx, amp_buffer->getBD());
        CHECKAMPERRLOG(err, "register BD to ADEC at output port");
      }
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief unregister AMP BD
 * @param port port structure point
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::unregisterBds(OmxAmpAudioPort *port) {
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
    if (NULL != bd) {
      if (port->isInput()) {
        if (mAudioDrm == OMX_FALSE) {
          AMP_RPC(err, AMP_ADEC_UnregisterBD, mAmpHandle, portIo, uiPortIdx, bd);
          CHECKAMPERRLOG(err, "unregister BD for ADEC");
        }
      } else {
        AMP_RPC(err, AMP_ADEC_UnregisterBD, mAmpHandle, portIo, uiPortIdx, bd);
        CHECKAMPERRLOG(err, "unregister BD for ADEC");
      }
    }
  }
  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioDecoder::resetPtsForFirstFrame(
    OMX_BUFFERHEADERTYPE *in_head) {
  // TODO: This is workaround to give a pts, but it's not accurate,
  // TODO: need player give right pts
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mOutputPort->isTunneled()) {
    OMX_COMPONENTTYPE *aren =
        static_cast<OMX_COMPONENTTYPE *>(mOutputPort->getTunnelComponent());
    OmxPortImpl *clk_port = static_cast<OmxComponentImpl *>(
        aren->pComponentPrivate)->getPort(kClockPortStartNumber);
    if (clk_port != NULL && clk_port->isTunneled()) {
      OMX_COMPONENTTYPE *clock =
          static_cast<OMX_COMPONENTTYPE *>(clk_port->getTunnelComponent());
      OMX_TIME_CONFIG_CLOCKSTATETYPE state;
      OMX_U32 wait_count = 20;
      InitOmxHeader(&state);
      while (wait_count > 0) {
        err = OMX_GetConfig(clock,
            OMX_IndexConfigTimeClockState, &state);
        CHECKOMXERR(err);
        if (OMX_TIME_ClockStateRunning == state.eState) {
          in_head->nTimeStamp = state.nStartTime;
          OMX_LOGD("Invalid pts, Reset the first frame pts %lld", in_head->nTimeStamp);
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
  OMX_LOG_FUNCTION_EXIT;
  return err;
}


/**
 * @brief AMP ADEC callback
 * @param component AMP DEC component
 * @param port_Io AMP port info
 * @param port_idx port index
 * @param bd AMP BD info
 * @param context OmxAmpAudioDecoder
 * @return value
 */
HRESULT OmxAmpAudioDecoder::pushBufferDone(
    AMP_COMPONENT component,
    AMP_PORT_IO port_Io,
    UINT32 port_idx,
    AMP_BD_ST *bd, void *context) {
  HRESULT err = SUCCESS;
  OMX_BUFFERHEADERTYPE *buf_header = NULL;
  OMX_BOOL isLastOutFrame = OMX_FALSE;
  OmxAmpAudioDecoder *decoder = static_cast<OmxAmpAudioDecoder *>(context);
  if (CheckMediaHelperEnable(decoder->mMediaHelper,
      PUSH_BUFFER_DONE) == OMX_TRUE) {
    OMX_LOGD("pushBufferDone, port_IO %d, bd 0x%x, bd.bdid 0x%x, bd.allocva 0x%x",
        port_Io, bd, bd->uiBDId, bd->uiAllocVA);
  }
  if (AMP_PORT_INPUT == port_Io && 0 == port_idx) {
    if (NULL != decoder->mPool) {
      OMX_U32 ret;
      // dump cbuf in bd back es data
      if (decoder->mWMDRM != OMX_TRUE && CheckMediaHelperEnable(decoder->mMediaHelper,
          DUMP_AUDIO_CBUF_DONE_ES) == OMX_TRUE) {
        AMP_BDTAG_MEMINFO *pMemInfo;
        AMP_BDTAG_GetWithIndex(bd, 0, (VOID **)&pMemInfo);
        void *pVirAddr = NULL;
        AMP_SHM_GetVirtualAddress(pMemInfo->uMemHandle, 0, &pVirAddr);
        mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_ES,
            reinterpret_cast<OMX_U8 *>(pVirAddr) + pMemInfo->uMemOffset,
            pMemInfo->uSize);
      }
      ret = AMP_CBUFRelease(decoder->mPool, bd);
      if (AMPCBUF_SUCCESS == ret) {
        decoder->mReturnedBdNum++;
        if (CheckMediaHelperEnable(decoder->mMediaHelper,
            PRINT_BD_IN_BACK) == OMX_TRUE) {
          OMX_LOGD("Cbuf [%u/%u] Return Bd from Amp", decoder->mReturnedBdNum,
              decoder->mPushedBdNum);
        }
      } else {
        OMX_LOGE("Cbuf Release Bd error %u.", ret);
      }
      return err;
    }
    buf_header = (static_cast<OmxAmpAudioPort *>(decoder->mInputPort))
        ->getBufferHeader(bd);
    if (NULL != buf_header) {
      decoder->returnBuffer(decoder->mInputPort, buf_header);
      decoder->mInBDBackNum++;
      if (CheckMediaHelperEnable(decoder->mMediaHelper,
          PRINT_BD_IN_BACK) == OMX_TRUE) {
        OMX_LOGD("[In] %d/%d", decoder->mInBDBackNum, decoder->mInputFrameNum);
      }
    }
  } else if (AMP_PORT_OUTPUT == port_Io && 0 == port_idx) {
    buf_header = (static_cast<OmxAmpAudioPort *>(decoder->mOutputPort))
        ->getBufferHeader(bd);
    if (NULL != buf_header) {
      AmpBuffer *amp_buffer =  static_cast<AmpBuffer *>(
          buf_header->pPlatformPrivate);
      if (decoder->mMark.hMarkTargetComponent != NULL) {
        buf_header->hMarkTargetComponent = decoder->mMark.hMarkTargetComponent;
        buf_header->pMarkData = decoder->mMark.pMarkData;
        decoder->mMark.hMarkTargetComponent = NULL;
        decoder->mMark.pMarkData = NULL;
      }
      AMP_BDTAG_AUD_FRAME_INFO *frame_info = NULL;
      err = amp_buffer->getAudioFrameInfo(&frame_info);
      if (frame_info == NULL) {
        OMX_LOGE("Get frame info return err 0x%x", err);
        return err;
      }
      if (frame_info->uFlag & AMP_MEMINFO_FLAG_EOS_MASK ||
          decoder->mDelayOutputEOS == OMX_TRUE) {
        if (frame_info->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
          OMX_LOGD("[Out] Receive EOS");
          frame_info->uFlag = 0;
        }
        // for mp3 to pass cts, we need delay to set mOutputEOS
        // in order to fill padding size in the latter empty output buffer
        // as ACodec would skip the buffer after padding the data larger than frame size
        if (decoder->mDelayOutputEOS == OMX_FALSE &&
            decoder->mInputPort->getAudioDefinition().eEncoding ==
            OMX_AUDIO_CodingMP3 && frame_info->uDataLength > 0) {
          OMX_LOGD("[Out] MP3 Delay send out EOS");
          decoder->mDelayOutputEOS = OMX_TRUE;
        } else {
          buf_header->nFlags |= OMX_BUFFERFLAG_EOS;
          decoder->mOutputEOS = OMX_TRUE;
          decoder->postEvent(OMX_EventBufferFlag, buf_header->nOutputPortIndex,
              OMX_BUFFERFLAG_EOS);
          decoder->mDelayOutputEOS = OMX_FALSE;
        }
      }
      AMP_BDTAG_AVS_PTS *pts_tag = NULL;
      err = amp_buffer->getOutputPts(&pts_tag);
      if (pts_tag == NULL) {
        OMX_LOGE("Get getOutputPts return 0x%x", err);
      }
      #if 0
      OMX_TICKS pts_low, pts_high;
      pts_low = static_cast<OMX_TICKS>(frame_info->uiPtsLow);
      pts_high = static_cast<OMX_TICKS>(frame_info->uiPtsHigh);
      if (frame_info->uiPtsHigh & TIME_STAMP_VALID_MASK) {
        buf_header->nTimeStamp = (pts_high & 0x7FFFFFFF) << 32 | pts_low;
      }
      #endif
      void *pVirAddr = NULL;
      AMP_SHM_GetVirtualAddress(frame_info->uMemHandle, 0, &pVirAddr);

      if (pts_tag == NULL) {
        buf_header->nTimeStamp = decoder->mTimeStampUs;
      } else {
        buf_header->nTimeStamp = convert90KtoUs(pts_tag->uPtsHigh, pts_tag->uPtsLow);
      }
      buf_header->nOffset = frame_info->uMemOffset[0];
      buf_header->nFilledLen = frame_info->uDataLength;
      buf_header->pBuffer = reinterpret_cast<OMX_U8 *>(pVirAddr);
      if (decoder->mOutputFrameNum == 0) {
        buf_header->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        if (decoder->mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3) {
          OMX_U32 channel_num = 0;
          static_cast<OmxAmpAudioPort *>(decoder->mOutputPort)->GetOutputParameter(
              NULL, &channel_num);
          OMX_U32 pad_size = kMp3DecoderDelay * channel_num * sizeof(OMX_S16);
          buf_header->nOffset += pad_size;
          if (buf_header->nFilledLen >= pad_size) {
            buf_header->nFilledLen -= pad_size;
          }
        }
      }

      // TODO: workaround for amp filled buffer size after EOS
      if (decoder->mOutputEOS) {
        if ((buf_header->nFlags & OMX_BUFFERFLAG_EOS) != 0
            && decoder->mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3
            && decoder->mOutputFrameNum > 0) {
          OMX_U32 channel_num = 0;
          static_cast<OmxAmpAudioPort *>(decoder->mOutputPort)->GetOutputParameter(
              NULL, &channel_num);
          OMX_U32 pad_size = kMp3DecoderDelay * channel_num * sizeof(OMX_S16);
          // in order to pass mp3 cts test case, we add this workaround
          if (buf_header->nAllocLen > buf_header->nFilledLen) {
            if (buf_header->nAllocLen >= buf_header->nFilledLen + pad_size) {
              kdMemset(buf_header->pBuffer + buf_header->nFilledLen, 0, pad_size);
              buf_header->nFilledLen += pad_size;
            } else {
              OMX_LOGE("There is no enough space to padding data");
            }
          }
        }
      }
      // in case AMP audio can not return correct pts, would remove later
      if (decoder->mTimeStampUs == -1) {
        decoder->mTimeStampUs = 0;
      }
      if (buf_header->nTimeStamp <= 0 && decoder->mTimeStampUs > 0) {
        buf_header->nTimeStamp = decoder->mTimeStampUs;
      }

      if (frame_info->uSampleRate > 0) {
        decoder->mTimeStampUs += static_cast<OMX_TICKS>(
            (1000000LL * frame_info->uDataLength)) /
            (frame_info->uSampleRate * kDefaultChannels * 2);  // the 2 is bitdepts
      } else {
        decoder->mTimeStampUs += static_cast<OMX_TICKS>(
            (1000000LL * frame_info->uDataLength) /
            (kDefaultSampleRate * kDefaultChannels * 2));  // the 2 is bitdepts
      }

      if (decoder->mOutputConfigured == OMX_FALSE &&
          frame_info->uDataLength > 0) {
        OMX_U32 mSamplerate = 0;
        OMX_U32 mChannel = 0;
        (static_cast<OmxAmpAudioPort *>(decoder->mOutputPort))->GetOutputParameter(
            &mSamplerate, &mChannel);
        if (frame_info->uSampleRate != mSamplerate ||
            frame_info->uChanNr != mChannel) {
          OMX_LOGD("Output port changed from samplerate %d channel %d to sr %d channel num %d\n",
              mSamplerate, mChannel, frame_info->uSampleRate, frame_info->uChanNr);
          (static_cast<OmxAmpAudioPort *>(decoder->mOutputPort))->SetOutputParameter(
              frame_info->uSampleRate, frame_info->uChanNr);
          decoder->mOutputFrameNum++;
          // unregisterBds output BD
          decoder->unregisterBds(static_cast<OmxAmpAudioPort *>(decoder->mOutputPort));
          decoder->postEvent(OMX_EventPortSettingsChanged, buf_header->nOutputPortIndex, 0);
          decoder->mOutputConfigured = OMX_TRUE;
          return SUCCESS;
        }
        decoder->mOutputConfigChangedComplete = OMX_TRUE;
      }

      // dump pcm data
      if (CheckMediaHelperEnable(decoder->mMediaHelper,
          DUMP_AUDIO_FRAME) == OMX_TRUE) {
        mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_FRMAE,
            reinterpret_cast<OMX_U8 *>(pVirAddr), frame_info->uDataLength);
      }
      if (CheckMediaHelperEnable(decoder->mMediaHelper,
          PRINT_BD_OUT_BACK) == OMX_TRUE) {
        OMX_LOGD("[Out] %d/%d, size %d, len %d, offset 0 %d shm %x buf num %d "
            "get pts 90K %0x us %lld calu pts "TIME_FORMAT
            " framerate %d channel num %d",
            decoder->mOutputFrameNum, decoder->mOutBDPushNum,
            frame_info->uSize, buf_header->nFilledLen,
            buf_header->nOffset, frame_info->uMemHandle,
            frame_info->uBufNr,
            (pts_tag != NULL) ? pts_tag->uPtsLow : 0, buf_header->nTimeStamp,
            TIME_ARGS(decoder->mTimeStampUs),
            frame_info->uSampleRate, frame_info->uChanNr);
      }
      decoder->returnBuffer(decoder->mOutputPort, buf_header);
      decoder->mOutputFrameNum++;
    }
  } else {
    OMX_LOGE("bd not be used");
    err = ERR_NOTIMPL;
    return err;
  }
  return err;
}

OMX_ERRORTYPE OmxAmpAudioDecoder::DecryptSecureDataToCbuf(
    OMX_BUFFERHEADERTYPE *in_head,
    CryptoInfo *cryptoInfo,
    AMP_SHM_HANDLE handle,
    OMX_U32 cbuf_offset) {
  HRESULT err = SUCCESS;
#ifndef DISABLE_PLAYREADY
  if (NULL != cryptoInfo) {
    OMX_U32 head_offset = 0;
    for (OMX_U32 i = 0; i < cryptoInfo->mNumSubSamples; i++) {
      OMX_U32 numBytesOfClearData =
          cryptoInfo->mSubSamples[i].mNumBytesOfClearData;
      OMX_U32 numBytesOfEncryptedData =
          cryptoInfo->mSubSamples[i].mNumBytesOfEncryptedData;
      if (CheckMediaHelperEnable(mMediaHelper, AUDIO_SECURE_INFO) == OMX_TRUE) {
        OMX_LOGD("Subsample %d  clear %d  encrypted %d", i, numBytesOfClearData,
            numBytesOfEncryptedData);
      }
      if (0u < numBytesOfClearData) {
      #ifdef WMDRM_SUPPORT
        err = MV_DRM_WMDRM_Content_Decrypt(NULL,
            (char *)(in_head->pBuffer + in_head->nOffset + head_offset),
            numBytesOfClearData,
            0,
            handle,
            cbuf_offset);
      #endif
        if (CheckMediaHelperEnable(mMediaHelper, AUDIO_SECURE_INFO) == OMX_TRUE) {
          OMX_LOGD("Decrypt clear  offset %d  size %d", cbuf_offset, numBytesOfClearData);
        }
        if (SUCCESS != err) {
          OMX_LOGE("Decrypt clear data err %d", err);
        }
        head_offset += numBytesOfClearData;
        cbuf_offset += numBytesOfClearData;
      }
      if (0u < numBytesOfEncryptedData) {
      #ifdef WMDRM_SUPPORT
        err = MV_DRM_WMDRM_Content_Decrypt(NULL,
            (char *)(in_head->pBuffer + in_head->nOffset + head_offset),
            numBytesOfEncryptedData,
            1,
            handle,
            cbuf_offset);
      #endif
        if (CheckMediaHelperEnable(mMediaHelper, AUDIO_SECURE_INFO) == OMX_TRUE) {
          OMX_LOGD("Decrypt encrypted  offset %d  size %d", cbuf_offset,
              numBytesOfEncryptedData);
        }
        if (SUCCESS != err) {
          OMX_LOGE("Decrypt clear data err %d", err);
        }
        head_offset += numBytesOfEncryptedData;
        cbuf_offset += numBytesOfEncryptedData;
      }
    }
  } else {
    OMX_LOGV("Secure WM frame with No cryptoInfo...");
    #ifdef WMDRM_SUPPORT
    err = MV_DRM_WMDRM_Content_Decrypt(NULL,
        (char *)(in_head->pBuffer + in_head->nOffset),
        in_head->nFilledLen,
        0,
        handle,
        cbuf_offset);
    #endif
    if (CheckMediaHelperEnable(mMediaHelper, AUDIO_SECURE_INFO) == OMX_TRUE) {
      OMX_LOGD("Decrypt clear  offset %d  size %d", cbuf_offset, in_head->nFilledLen);
    }
    if (SUCCESS != err) {
      OMX_LOGE("Decrypt clear data err %d", err);
    }
  }
#endif
  return static_cast<OMX_ERRORTYPE>(err);
}

/**
 * @brief: Push bd to Cbuf
 */
UINT32 OmxAmpAudioDecoder::pushCbufBd(OMX_BUFFERHEADERTYPE *in_head,
    CryptoInfo *cryptoInfo) {
  UINT32 ret, offset;
  OMX_U32 flag = (mEOS == OMX_TRUE) ? AMP_MEMINFO_FLAG_EOS_MASK : 0;
  UINT32 data_len = in_head->nFilledLen;
  if (OMX_AUDIO_CodingRA == mInputPort->getAudioDefinition().eEncoding) {
    // Data padding is needed for block alignment.
    if (in_head->nFilledLen % 2) {
      data_len = in_head->nFilledLen + 1;
    }
  }
#ifdef OMX_AudioExt_h
    else if (OMX_AUDIO_CodingDTS == mInputPort->getAudioDefinition().eEncoding) {
    // Data padding is needed for block alignment.
    // DTS max frame size is 32K, define dts buffer size 32K+64 for align
    // If frame length less then 2048, padding to 2048 else padding align with 4 bytes
    if (in_head->nFilledLen < 2048) {
      data_len = 2048;
      kdMemset(in_head->pBuffer + in_head->nOffset + in_head->nFilledLen,
          2048 - in_head->nFilledLen, 0x0);
    } else if (in_head->nFilledLen % 4) {
      if (in_head->nFilledLen + 4 - in_head->nFilledLen % 4 <=
          mInputPort->mDefinition.nBufferSize) {
        data_len = in_head->nFilledLen + 4 - in_head->nFilledLen % 4;
        kdMemset(in_head->pBuffer + in_head->nOffset + in_head->nFilledLen,
          4 - in_head->nFilledLen % 4, 0x0);
      } else {
        OMX_LOGE("Invalid DTS frame length %d", in_head->nFilledLen);
      }
    }
  }
#endif
  ret = AMP_CBUFWriteData(mPool, in_head->pBuffer + in_head->nOffset,
      data_len, flag, &offset);
  if (AMPCBUF_SUCCESS == ret) {
    if (mWMDRM == OMX_TRUE) {
      AMP_SHM_HANDLE mshm;
      AMP_CBUFGetShm(mPool, &mshm);
      DecryptSecureDataToCbuf(in_head, cryptoInfo, mshm, offset);
    }
    mInputFrameNum ++;
    if (mEOS == OMX_TRUE) {
      OMX_LOGD("Cbuf push EOS %d\n", flag);
    }
    // dump cbuf es data
    if (CheckMediaHelperEnable(mMediaHelper, DUMP_AUDIO_ES) == OMX_TRUE) {
      mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_ES,
          in_head->pBuffer + in_head->nOffset, in_head->nFilledLen);
    }
    if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN) == OMX_TRUE) {
      OMX_LOGD("<In> %u/%u, size %d offset %d \tpts "TIME_FORMAT "\tflag %s ",
          mInBDBackNum, mInputFrameNum, in_head->nFilledLen, in_head->nOffset,
          TIME_ARGS(in_head->nTimeStamp),
          GetOMXBufferFlagDescription(in_head->nFlags));
    }
    AMP_BDTAG_UNITSTART pts_tag;
    kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_UNITSTART));
    pts_tag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL,
        sizeof(AMP_BDTAG_UNITSTART)};
    convertUsto90K(in_head->nTimeStamp, &pts_tag.uPtsHigh,
        &pts_tag.uPtsLow);
    pts_tag.uStrmPos = offset;
    AMP_CBUFInsertTag(mPool, reinterpret_cast<UINT8 *>(&pts_tag),
        offset);

    if (OMX_AUDIO_CodingRA == mInputPort->getAudioDefinition().eEncoding) {
      AMP_BDTAG_AUD_REALAUDIO RaDecInfo;
      kdMemset(&RaDecInfo, 0, sizeof(AMP_BDTAG_AUD_REALAUDIO));
      RaDecInfo.Header.eType = AMP_BDTAG_AUD_REALAUDIO_DEC;
      RaDecInfo.Header.uLength = sizeof(RaDecInfo);
      RaDecInfo.m_BlockFlag = 0xFFFFFFFF;
      RaDecInfo.m_BlockSize = in_head->nFilledLen;
      RaDecInfo.m_TimeStamp = 0;
      AMP_CBUFInsertTag(mPool, reinterpret_cast<UINT8 *>(&RaDecInfo), offset);
    }

    returnBuffer(mInputPort, in_head);
    mCachedhead = NULL;
    mInBDBackNum++;
  } else if (AMPCBUF_LACKSPACE == ret) {
    OMX_LOGV("Cbuf Lack of space.");
  } else {
    mInputFrameNum ++;
    OMX_LOGE("Cbuf already Reached Eos!");
    returnBuffer(mInputPort, in_head);
    mCachedhead = NULL;
    mInBDBackNum++;
  }
  return ret;
}

/**
 * @brief: Get bd from Cbuf
 */
UINT32 OmxAmpAudioDecoder::requestCbufBd() {
  AMP_BD_ST *bd;
  UINT32 ret, offset, framesize;
  ret = AMP_CBUFRequest(mPool, &bd, &offset, &framesize);
  if (AMPCBUF_SUCCESS == ret) {
    mPushedBdNum++;
     if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN) == OMX_TRUE) {
      OMX_LOGD("Cbuf <%u/%u> Push Bd to Amp: offset %u  size %u",
          mReturnedBdNum, mPushedBdNum, offset, framesize);
    }
    // dump es data
    if (mWMDRM != OMX_TRUE
        && CheckMediaHelperEnable(mMediaHelper, DUMP_AUDIO_CBUF_ES) == OMX_TRUE) {
      AMP_SHM_HANDLE mshm;
      AMP_CBUFGetShm(mPool, &mshm);
      void *pVirAddr = NULL;
      AMP_SHM_GetVirtualAddress(mshm, 0, &pVirAddr);
      mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_ES,
          reinterpret_cast<OMX_U8 *>(pVirAddr) + offset, framesize);
    }
    pushAmpBd(AMP_PORT_INPUT, 0, bd);
  } else if (AMPCBUF_LACKBD == ret) {
    OMX_LOGV("Cbuf Lack of Bd, waitting.");
  } else if (AMPCBUF_LACKDATA == ret) {
    OMX_LOGV("Cbuf Lack of Data, waitting.");
  } else {
    OMX_LOGD("Cbuf Request Bd err %d.", ret);
  }
  return ret;
}


/**
 * @brief push buffer thread, process input port and output port
 */
void OmxAmpAudioDecoder::pushBufferLoop() {
  OMX_BUFFERHEADERTYPE *in_head = NULL, *out_head = NULL;
  OMX_BOOL isCaching = OMX_FALSE;
 Loop_start:
  while (!mShouldExit) {
    // Get input Buffer
    while (!mShouldExit && (!mInputPort->isEmpty() || NULL != mCachedhead)) {
      kdThreadMutexLock(mBufferLock);
      if (NULL != mCachedhead) {
        OMX_LOGV("Got input buffer %p", in_head);
        in_head = mCachedhead;
        isCaching = OMX_TRUE;
        OMX_LOGV("Use the cached input buffer %p", in_head);
      } else {
        in_head = mInputPort->popBuffer();
        if (mPool != NULL) {
          mCachedhead = in_head;
          isCaching = OMX_FALSE;
        }
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
        if (in_head->nFilledLen <= 0 && !(in_head->nFlags & OMX_BUFFERFLAG_EOS)) {
          OMX_LOGD("Invalid frame size %d, return buffer", in_head->nFilledLen);
          returnBuffer(mInputPort, in_head);
          in_head = NULL;
          mCachedhead = NULL;
          kdThreadMutexUnlock(mBufferLock);
          continue;
        }
        // workaround for nosie issue by skip invalid mp3 frame, but this should be done in decoder
        // This may has issue that if the frame header is not at the beginning
        // To control affect, only handle the mpeg audio in avi container
        if (!static_cast<OmxAmpAudioPort *>(mInputPort)->mOmxDisableTvp
            && mInputFrameNum == 0
            && mContainerType.eFmtType == OMX_FORMAT_AVI
            && (mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3
                || mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP2
                || mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP1)) {
          if ((!(in_head->nFlags & OMX_BUFFERFLAG_EOS)) &&
              (CheckMp3FrameValid(in_head->pBuffer + in_head->nOffset,
              in_head->nFilledLen) == OMX_FALSE)) {
            OMX_LOGD("mp3 frame size %d is invalid, skip", in_head->nFilledLen);
            returnBuffer(mInputPort, in_head);
            in_head = NULL;
            mCachedhead = NULL;
            kdThreadMutexUnlock(mBufferLock);
            continue;
          }
        }
        // Process start frame
        if (in_head->nFlags & OMX_BUFFERFLAG_STARTTIME) {
        }
        if (!isCaching && in_head->nFlags & OMX_BUFFERFLAG_EOS) {
          OMX_LOGD("Meet EOS Frame, wait for stop");
          mEOS = OMX_TRUE;
        }
        if (in_head->hMarkTargetComponent != NULL) {
          mMark.hMarkTargetComponent = in_head->hMarkTargetComponent;
          mMark.pMarkData = in_head->pMarkData;
        }
        // Process codec config data
        if (in_head->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
          if (!static_cast<OmxAmpAudioPort *>(mInputPort)->mOmxDisableTvp) {
            if (mCodecSpecficDataSent) {
              vector<CodecSpeficDataType>::iterator it;
              while (!mCodecSpeficData.empty()) {
                it = mCodecSpeficData.begin();
                if (it->data != NULL) {
                  kdFree(it->data);
                }
                mCodecSpeficData.erase(it);
              }
            }
            CodecSpeficDataType extra_data;
            extra_data.data = kdMalloc(in_head->nFilledLen);
            extra_data.size = in_head->nFilledLen;
            char *pData = reinterpret_cast<char *>(in_head->pBuffer)
                + in_head->nOffset;
            kdMemcpy(extra_data.data, in_head->pBuffer + in_head->nOffset,
                in_head->nFilledLen);
            mCodecSpeficData.push_back(extra_data);
            in_head->nFilledLen = 0;
            in_head->nOffset = 0;
            returnBuffer(mInputPort, in_head);
            mCachedhead = NULL;
            // use esds data to generate adts header
            OmxPortImpl* port = getPort(kAudioPortStartNumber);
            if (port != NULL &&
                port->getDomain() == OMX_PortDomainAudio) {
              if (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
                static_cast<OmxAmpAacPort*>(port)->setCodecParam(NULL,
                    reinterpret_cast<OMX_U8 *>(extra_data.data));
              } else if (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingWMA) {
                setWMAParameter(reinterpret_cast<OMX_U8 *>(extra_data.data),
                    extra_data.size);
              }
            }
          }
          kdThreadMutexUnlock(mBufferLock);
          continue;
        }
        CryptoInfo *cryptoInfo = NULL;
        OMX_OTHER_EXTRADATA_TIME_INFO *timeInfo = NULL;
        if ((!static_cast<OmxAmpAudioPort *>(mInputPort)->mOmxDisableTvp
                && mOutputPort->isTunneled()
                && (in_head->nFlags & OMX_BUFFERFLAG_EXTRADATA))
            || (!static_cast<OmxAmpAudioPort *>(mInputPort)->mOmxDisableTvp
                && !mOutputPort->isTunneled()
                && mAudioDrm)) {
          OMX_U32 offset = (in_head->nOffset +in_head->nFilledLen + 3) / 4 * 4;
          OMX_OTHER_EXTRADATATYPE *extraData =
              reinterpret_cast<OMX_OTHER_EXTRADATATYPE *>(in_head->pBuffer + offset);
          while (OMX_ExtraDataNone != extraData->eType) {
            if (static_cast<OMX_EXTRADATATYPE>(OMX_ExtraDataCryptoInfo) ==
                extraData->eType) {
              cryptoInfo = reinterpret_cast<CryptoInfo *>(extraData->data);
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
        }
        if (mInited == OMX_FALSE) {
          if (mInputPort->getAudioDefinition().eEncoding == OMX_AUDIO_CodingVORBIS) {
            OMX_U32 codec_data_size = 0;
            vector<CodecSpeficDataType>::iterator it;
            for (it = mCodecSpeficData.begin();
                it != mCodecSpeficData.end(); it++) {
              codec_data_size += it->size;
            }
            OMX_U8 *data = in_head->pBuffer + in_head->nOffset;
            kdMemmove(data + codec_data_size, data, in_head->nFilledLen);
            OMX_U32 offset = 0;
            for (it = mCodecSpeficData.begin();
                it != mCodecSpeficData.end(); it++) {
              OMX_LOGD("Merge extra data %p, size %d", it->data, it->size);
              kdMemcpy(data + offset, it->data, it->size);
              offset += it->size;
            }
            in_head->nFilledLen += codec_data_size;
          }
          mCodecSpecficDataSent = OMX_TRUE;
          mInited = OMX_TRUE;
        }
        if (mPool == NULL) {
          mInputFrameNum++;
        }
        if (in_head->nTimeStamp < 0 && mStreamPosition == 0) {
            OMX_LOGD("<In> timestamp "TIME_FORMAT "invalid",
                TIME_ARGS(in_head->nTimeStamp));
            resetPtsForFirstFrame(in_head);
        }
        AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
            in_head->pPlatformPrivate);
        // update pts when initial or after seek
        if (mTimeStampUs == -1) {
          mTimeStampUs = in_head->nTimeStamp;
        }
        OMX_U8 header_size = 0;
        if (mAudioDrm == OMX_TRUE) {
          amp_buffer->clearBdTag();
          amp_buffer->addDmxUnitStartTag(in_head, mStreamPosition);
          amp_buffer->addMemInfoTag(in_head,
              static_cast<OmxAmpAudioPort *>(mInputPort)->mPushedOffset);
          static_cast<OmxAmpAudioPort *>(mInputPort)->updateMemInfo(in_head);
          if (NULL != cryptoInfo) {
            OMX_U64 encryptionOffset = 0;
            OMX_U64 initial_iv;
            OMX_U64 initial_blockoffset;
            if (mSchemeIdSend == OMX_FALSE) {
              amp_buffer->addCryptoCtrlInfoTag(mDrm_type, mStreamPosition);
              mSchemeIdSend = OMX_TRUE;
            }
            kdMemcpy(&initial_iv, cryptoInfo->mIv, sizeof(OMX_U64));
            kdMemcpy(&initial_blockoffset, cryptoInfo->mIv + 8, sizeof(OMX_U64));
            for (OMX_U32 i = 0; i < cryptoInfo->mNumSubSamples; i++) {
              OMX_U32 numBytesOfClearData =
                  cryptoInfo->mSubSamples[i].mNumBytesOfClearData;
              OMX_U32 numBytesOfEncryptedData =
                  cryptoInfo->mSubSamples[i].mNumBytesOfEncryptedData;
              if (CheckMediaHelperEnable(mMediaHelper,
                  AUDIO_SECURE_INFO) == OMX_TRUE) {
                OMX_LOGD("mMode is %d crypto info num %d clear size %d encrypted size %d\n",
                    cryptoInfo->mMode, cryptoInfo->mNumSubSamples, numBytesOfClearData,
                    numBytesOfEncryptedData);
              }
              if (0u < numBytesOfClearData) {
                amp_buffer->addDmxCtrlInfoTag(mAudioLength, numBytesOfClearData,
                    OMX_FALSE, 0, 0, 0, mDrm_type);
                mAudioLength += numBytesOfClearData;
              }
              if (0u < numBytesOfEncryptedData) {
                if (mDrm_type == AMP_DRMSCHEME_MARLIN) {
                  OMX_U64 iv = initial_iv;
                  OMX_U64 blockoffset = initial_blockoffset;
                  memReverse((OMX_U8 *)(&iv), sizeof(OMX_U64));
                  memReverse((OMX_U8 *)(&blockoffset), sizeof(OMX_U64));
                  blockoffset += (encryptionOffset >> 4);
                  amp_buffer->addDmxCtrlInfoTag(mAudioLength, numBytesOfEncryptedData,
                      OMX_TRUE, iv, blockoffset, encryptionOffset & 0x0f, mDrm_type);
                } else {
                  amp_buffer->addDmxCtrlInfoTag(mAudioLength, numBytesOfEncryptedData,
                      OMX_TRUE, initial_iv, encryptionOffset >> 4, encryptionOffset & 0x0f,
                      mDrm_type);
                }
                encryptionOffset += numBytesOfEncryptedData;
                mAudioLength += numBytesOfEncryptedData;
              }
            }
          } else {
            amp_buffer->addDmxCtrlInfoTag(mAudioLength, in_head->nFilledLen,
                OMX_FALSE, 0, 0, 0, mDrm_type);
            mAudioLength += in_head->nFilledLen;
          }
          mStreamPosition += in_head->nFilledLen;
        } else {
          if (NULL != mPool) {
            if (!isCaching) {
              header_size = (static_cast<OmxAmpAudioPort *>(
                  mInputPort))->formatAudEsData(in_head);
            }

            UINT32 ret;
            ret = requestCbufBd();
            if (AMPCBUF_LACKBD == ret) {
              OMX_LOGV("Cbuf Lack of Bd, waitting.");
              kdThreadMutexUnlock(mBufferLock);
              usleep(kDefaultLoopSleepTime);
              continue;
            }

            ret = pushCbufBd(in_head, cryptoInfo);
            if (AMPCBUF_LACKSPACE == ret) {
              OMX_LOGV("Cbuf Lack of space, waitting.");
              kdThreadMutexUnlock(mBufferLock);
              usleep(kDefaultLoopSleepTime);
              continue;
            }
            // request the EOS bd and push to ADEC
            if (mEOS == OMX_TRUE) {
              requestCbufBd();
            }
          } else {
            amp_buffer->updatePts(in_head, mStreamPosition);
            header_size = (static_cast<OmxAmpAudioPort *>(
                mInputPort))->formatAudEsData(in_head);
            amp_buffer->updateMemInfo(in_head);
          }
          if (isCaching != OMX_TRUE) {
            mStreamPosition += in_head->nFilledLen;
          }
        }
        if (mPool == NULL) {
          // dump es data
          if (!static_cast<OmxAmpAudioPort *>(mInputPort)->mOmxDisableTvp
              && CheckMediaHelperEnable(mMediaHelper, DUMP_AUDIO_ES) == OMX_TRUE) {
            mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_ES,
                in_head->pBuffer + in_head->nOffset, in_head->nFilledLen);
          }
          if ((CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN) == OMX_TRUE)) {
            OMX_LOGD("<In> %u/%u, size %d offset %d\tpts "TIME_FORMAT "\tflag %s ",
                mInBDBackNum, mInputFrameNum, in_head->nFilledLen,
                in_head->nOffset, TIME_ARGS(in_head->nTimeStamp),
                GetOMXBufferFlagDescription(in_head->nFlags));
          }
          pushAmpBd(AMP_PORT_INPUT, 0, amp_buffer->getBD());
        }
      } else {
        OMX_LOGE("get input buffer %p error", in_head);
      }
      kdThreadMutexUnlock(mBufferLock);
    }
    // Push output buffer to ADEC
    /* Make sure push only one output buffer to ADEC then wait for check the output port
      setting correct or not in pushBufferDone();
    */
    while (!mOutputPort->isEmpty() && mOutputEOS == OMX_FALSE &&
      ((mOutputConfigInit == OMX_FALSE && mOutputConfigChangedComplete == OMX_FALSE) ||
      (mOutputConfigInit == OMX_TRUE && mOutputConfigChangedComplete == OMX_TRUE))) {
      kdThreadMutexLock(mBufferLock);
      // set mOutputConfigInit that means have pushed one output BD to ADEC
      mOutputConfigInit = OMX_TRUE;
      // Output port have changed and unregisterBds before, need registerBds again.
      if (mOutputConfigured == OMX_TRUE) {
        registerBds(static_cast<OmxAmpAudioPort *>(mOutputPort));
        mOutputConfigured = OMX_FALSE;
      }
      if (mDelayOutputEOS == OMX_TRUE) {
        // make sure there is at least one output bd in amp
        OMX_PARAM_PORTDEFINITIONTYPE def;
        mOutputPort->getDefinition(&def);
        OMX_LOGD("mOutputFrameNum %d mOutBDPushNum %d", mOutputFrameNum, mOutBDPushNum);
        if (mPushCount == 0 && (mOutBDPushNum - mOutputFrameNum < 2)) {
          mPushCount ++;
        } else {
          mPushCount = 0;
          mOutputEOS = OMX_TRUE;
          HRESULT err = SUCCESS;
          OMX_LOGD("clear adec to return output buffer");
          // clear output port to let adec return output bd
          AMP_RPC(err, AMP_ADEC_ClearPortBuf, mAmpHandle, AMP_PORT_OUTPUT, 0);
          kdThreadMutexUnlock(mBufferLock);
          usleep(kDefaultLoopSleepTime);
          continue;
        }
      }
      out_head = mOutputPort->popBuffer();
      OMX_LOGV("Got output buffer %p", out_head);
      if (NULL != out_head) {
        if (mOutputPort->isFlushing()) {
          OMX_LOGD("Return output buffer %p when flushing", out_head);
          returnBuffer(mOutputPort, out_head);
          out_head = NULL;
          kdThreadMutexUnlock(mBufferLock);
          continue;
        }
        AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
            out_head->pPlatformPrivate);
        HRESULT err = SUCCESS;
        AMP_BDTAG_AUD_FRAME_INFO *frame_info = NULL;
        err = amp_buffer->getAudioFrameInfo(&frame_info);
        if (frame_info) {
          frame_info->uDataLength = 0;
        }
        if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_OUT) == OMX_TRUE) {
          OMX_LOGD("<Out> %d/%d,", mOutputFrameNum, mOutBDPushNum);
        }
        pushAmpBd(AMP_PORT_OUTPUT, 0, amp_buffer->getBD());
        mOutBDPushNum++;
      } else {
        OMX_LOGE("get output buffer %p error", out_head);
      }
      kdThreadMutexUnlock(mBufferLock);
    }
    usleep(kDefaultLoopSleepTime);
  }
  kdThreadMutexLock(mPauseLock);
  if (mPaused) {
    kdThreadCondWait(mPauseCond, mPauseLock);
    kdThreadMutexUnlock(mPauseLock);
    goto Loop_start;
  }
  kdThreadMutexUnlock(mPauseLock);
}

void OmxAmpAudioDecoder::onPortFlushCompleted(OMX_U32 portIndex) {

}

void OmxAmpAudioDecoder::onPortEnableCompleted(
    OMX_U32 portIndex, OMX_BOOL enabled) {
  if (portIndex == (kAudioPortStartNumber + 1) && enabled == OMX_TRUE) {
    mOutputConfigChangedComplete = OMX_TRUE;
  }
}

/**
 * @brief : create decode thread
 * @param args : OmxAmpAudioDecoder
 * @return : NULL
 */
void* OmxAmpAudioDecoder::threadEntry(void * args) {
  OmxAmpAudioDecoder *decoder = reinterpret_cast<OmxAmpAudioDecoder *>(args);
  prctl(PR_SET_NAME, "OmxAudioDecoder", 0, 0, 0);
  decoder->pushBufferLoop();
  return NULL;
}

/**
 * @brief create AMP audio decoder component
 * @param handle store OMX component handle
 * @param componentName OMX component name string
 * @param appData OMX component appData
 * @param callBacks OMX component callBacks
 * @return OMX_ErrorNone
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::createAmpAudioDecoder(
    OMX_HANDLETYPE *handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE *callBacks) {
  OMX_LOGD("createAmpAudioDecoder");
  OmxAmpAudioDecoder *comp = new OmxAmpAudioDecoder(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}

/**
 * @brief deinit component
 * @param hComponent OMX component handle
 */
OMX_ERRORTYPE OmxAmpAudioDecoder::componentDeInit(OMX_HANDLETYPE
    hComponent) {
  OMX_LOG_FUNCTION_ENTER;
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxAmpAudioDecoder *adec = static_cast<OmxAmpAudioDecoder *>(comp);
  delete adec;
  reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate = NULL;
  return OMX_ErrorNone;
}
void OmxAmpAudioDecoder::initAQ(OMX_S32 adec_type) {
  OMX_LOG_FUNCTION_ENTER;
  String8 FILE_SOURCE_URI;
  char ktmp[80] ;
  status_t ret;
  static const char* const kAudioSourcePrefix = "audio://localhost?decoder=";
  mAudioChannel = getAudioChannel();

  // TODO:some audio streams can't play by mooplayer but also use OMX.
  // In this case, it can't get RM and mAudioChannel = 0. AVSeting set the
  // audio channel is -0xff. it can use source_constants.h after chao's
  // code was merged: const int AUDIO_SOURCE_NONE_ID = -0xff;
  if ( 0 == mAudioChannel)
    mAudioChannel = - 0xff;

  mSourceControl = new SourceControl();
  if (mSourceControl != NULL) {
    sprintf(ktmp, "%s%d", kAudioSourcePrefix, mAudioChannel);
    FILE_SOURCE_URI = String8(ktmp);

    mSourceControl->getSourceByUri(FILE_SOURCE_URI, &mSourceId);
    OMX_LOGD("get sourceid :%d, sourceURL :%s", mSourceId, FILE_SOURCE_URI.string());
    if ( -1 != mSourceId) {
      // Notify soucecontrol to save the audio codec  when create pipline;
      AudioStreamInfo streamInfo;
      memset(&streamInfo, -1, sizeof(AudioStreamInfo));
      streamInfo.decoder_format = adec_type;
      mSourceControl->notifySourceAudioInfo(mSourceId, streamInfo);

      // Get DRC/Chanelmap/StereMode/DulMode values from soucecontrol
      mSourceControl->getDRCMode(mSourceId, &mPhyDRCMode);
      mSourceControl->getChannelMap(mSourceId, &mPhyChmap);
      mSourceControl->getStereoMode(mSourceId, &mStereoMode);
      mSourceControl->getDualMode(mSourceId, &mDualMode);
      OMX_LOGD("getDRC/chmap from avsetting, mPhyDRCMode :%0x,"
          "mPhyChmap :%0x, mStereoMode : %0x, mDualMode :%0x",
          mPhyDRCMode, mPhyChmap, mStereoMode, mDualMode);
    } else {
     OMX_LOGD("get source ID failed.");
    }
  }

  if (mOutputControl == NULL) {
    mOutputControl = new OutputControl();
    // After being registered , when AVSeting  changes DRC/StereoMode/DualMode value ,
    // we can get it by OnSettingUpdate() at once.
    if (mOutputControl != NULL) {
      mObserver = new AVSettingObserver(*this);
      ret = mOutputControl ->registerObserver("mrvl.output", mObserver);
      if (android::OK != ret) {
        OMX_LOGE("Rigister Observer failed !!!");
      }
    }
  }

  OMX_LOG_FUNCTION_EXIT;
}

OMX_S32 OmxAmpAudioDecoder::getAudioChannel() {
#if (defined(OMX_IndexExt_h) && defined(_OMX_GTV_))
    if (mResourceInfo.nResourceSize > 0) {
      OMX_LOGD("Resouce size %d, value 0x%x",
          mResourceInfo.nResourceSize, mResourceInfo.nResource[0]);
      return mResourceInfo.nResource[0];
    }
#endif
    return 0;
}

HRESULT OmxAmpAudioDecoder::registerEvent(AMP_EVENT_CODE event) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mListener) {
    err = AMP_Event_RegisterCallback(mListener, event, eventHandle, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "register ADEC notify");

    if (!err) {
      AMP_RPC(err, AMP_ADEC_RegisterNotify, mAmpHandle,
          AMP_Event_GetServiceID(mListener), event);
      CHECKAMPERRLOG(err, "register ADEC callback");
    }
  } else {
    OMX_LOGE("mListener is not created yet.");
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

HRESULT OmxAmpAudioDecoder::unRegisterEvent(AMP_EVENT_CODE event) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mListener) {
    AMP_RPC(err, AMP_ADEC_UnregisterNotify, mAmpHandle,
        AMP_Event_GetServiceID(mListener), event);
    CHECKAMPERRLOG(err, "unregister ADEC notify");

    if (!err) {
      err = AMP_Event_UnregisterCallback(mListener, event, eventHandle);
      CHECKAMPERRLOG(err, "unregister ADEC callback");
    }
  } else {
    OMX_LOGE("mListener is not created yet.");
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

HRESULT OmxAmpAudioDecoder::eventHandle(HANDLE hListener, AMP_EVENT *pEvent,
    VOID *pUserData) {
  if (!pEvent) {
    OMX_LOGE("pEvent is NULL!");
    return !SUCCESS;
  }
  AMP_ADEC_STRMINFO_EVENT *pStreamInfo = reinterpret_cast<AMP_ADEC_STRMINFO_EVENT *>(
      AMP_EVENT_PAYLOAD_PTR(pEvent));
  if (AMP_EVENT_ADEC_CALLBACK_STRMINFO == pEvent->stEventHead.eEventCode) {
    OmxAmpAudioDecoder *pComp = static_cast<OmxAmpAudioDecoder *>(pUserData);
    OMX_LOGD("event type:%d decformat:%ld streamformat:%d samplerate:%d channelnum:%d"
        "channelmode :%d lfe mode :%d bit depth:%d portindex: %d\n",
        pEvent->stEventHead.uiParam1,
        pStreamInfo->uiAdecFmt, pStreamInfo->uiStreamFmt,
        pStreamInfo->uiSampleRate, pStreamInfo->uiChannelNum,
        pStreamInfo->uiPriChanMode, pStreamInfo->uiLfeMode,
        pStreamInfo->uiBitDepth, pStreamInfo->iPortIdx);
    if ( pComp != NULL) {
      // Notify mooplayer after the first frame had been decoded.
      pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventIDECODED), 0, 0);
      if (-1 != pComp->mSourceId) {
        AudioStreamInfo streamInfo;
        streamInfo.decoder_format = pStreamInfo->uiAdecFmt;
        streamInfo.stream_format = pStreamInfo->uiStreamFmt;
        streamInfo.sample_rate = pStreamInfo->uiSampleRate;
        streamInfo.channels = pStreamInfo->uiChannelNum;
        streamInfo.pri_channel_mode = pStreamInfo->uiPriChanMode;
        streamInfo.lfe_mode = pStreamInfo->uiLfeMode;
        streamInfo.bit_depth = pStreamInfo->uiBitDepth;
        streamInfo.port_index = pStreamInfo->iPortIdx;
        pComp->mSourceControl->notifySourceAudioInfo(pComp->mSourceId, streamInfo);
        pComp->mSourceControl->applyAQ(pComp->mSourceId);
       }

      if (pStreamInfo->uiAdecFmt == AMP_DTS_HD) {
        HRESULT errCode = SUCCESS;
        AMP_ADEC_AUD_INFO audinfo;
        audinfo._d = AMP_ADEC_PARAIDX_DTS;
        AMP_RPC(errCode, AMP_ADEC_GetInfoWithFmt, pComp->mAmpHandle, AMP_DTS_HD, &audinfo);
        CHECKAMPERRLOGNOTRETURN(errCode, "AMP_ADEC_GetInfoWithFmt(AMP_MPG_AUDIO)");

        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventAudioDtsInfo),
            audinfo._u.stDtsInfo.uiStreamFormat, 0);
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventAudioDtsPlaybackMode),
            audinfo._u.stDtsInfo.uiPlaybackMode, 0);
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventAudioDtsChannelMask),
            audinfo._u.stDtsInfo.uiChannelMask, 0);
        pComp->postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventAudioDtsRealChannelMask),
            audinfo._u.stDtsInfo.uiRealChannelMask, 0);

        OMX_LOGD("dts, StreamFormat=%u, PlaybackMode=%u, ChannelMask=%u, RealChannelMask=%u",
            audinfo._u.stDtsInfo.uiStreamFormat,
            audinfo._u.stDtsInfo.uiPlaybackMode,
            audinfo._u.stDtsInfo.uiChannelMask,
            audinfo._u.stDtsInfo.uiRealChannelMask);
      }
    }
  }
  return SUCCESS;
}

void OmxAmpAudioDecoder::AVSettingObserver::OnSettingUpdate(const char* name,
    const AVSettingValue& value) {
  HRESULT err = SUCCESS;
  AMP_ADEC_PARAS mParas;
  OMX_AUDIO_CODINGTYPE coding_type = mAudioDecoder.mInputPort->getAudioDefinition().eEncoding;
  OMX_S32 amp_type = mAudioDecoder.getAmpDecType(coding_type);
  if (amp_type == AMP_MS11_DDC) {
    memset(&mParas, 0, sizeof(mParas));
    if (strstr(name, marvell::kPhyDrcMode)) {
      mParas._u.MS11_DDC.iDrcMode[0] = value.getInt();
      mParas._u.MS11_DDC.uiRTParaMask |= AMP_ADEC_MS11PARA_RT_DRCMODE_MAIN ;
      OMX_LOGD("New DRC value = %d from avsetting", value.getInt());
    } else if (strstr(name, marvell::kPhyStereoMode)) {
      mParas._u.MS11_DDC.iStereoMode = value.getInt();
      mParas._u.MS11_DDC.uiRTParaMask |= AMP_ADEC_MS11PARA_RT_STEREOMODE;
      OMX_LOGD("New StereoMode value = %d from avsetting", value.getInt());
    } else if (strstr(name, marvell::kPhyDualMode)) {
      mParas._u.MS11_DDC.iDualMode = value.getInt();
      OMX_LOGD("New DualMode value = %d from avsetting", value.getInt());
      mParas._u.MS11_DDC.uiRTParaMask |= AMP_ADEC_MS11PARA_RT_DUALMODE;
    }
    AMP_RPC(err, AMP_ADEC_SetParameters, mAudioDecoder.mAmpHandle,
          AMP_ADEC_PARAIDX_MS11_DDC, &mParas);
    CHECKAMPERRLOGNOTRETURN(err, "set AMP_ADEC_PARAIDX_MS11_DDC");
  }
}
}

