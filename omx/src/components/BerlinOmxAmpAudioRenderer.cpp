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


#include <BerlinOmxAmpAudioRenderer.h>

extern "C" {
#include <amp_sound_api.h>
#include <amp_event_listener.h>
#include <mediahelper.h>
}

#undef  LOG_TAG
#define LOG_TAG "BerlinOmxAmpAudioRenderer"

namespace berlin {

const char *OMX_ROLE_AUDIO_RENDERER_PCM = "audio_renderer.pcm";

// AMP port index starts from 0, define a start number to make code more readable.
static const UINT32 kAMPPortStartNumber = 0x0;
static const OMX_U32 stream_in_buf_size = 1 * 1024 * 1024;
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

OmxAmpAudioRenderer::OmxAmpAudioRenderer() {
  OMX_LOGD("Default constructor.");
}

OmxAmpAudioRenderer::OmxAmpAudioRenderer(OMX_STRING name)
  : mInputPort(NULL),
    mClockPort(NULL),
    mListener(NULL),
    mThread(0),
    mThreadAttr(NULL),
    mShouldExit(OMX_FALSE),
    mPaused(OMX_FALSE),
    mPauseCond(NULL),
    mPauseLock(NULL),
    mBufferLock(NULL),
    mNumChannels(0),
    mSamplingRate(0),
    mNumData(0),
    mBitPerSample(16),
    mEndian(0),
    mAudioMute(OMX_FALSE),
    mStreamPosition(0),
    mInputFrameNum(0),
    mInBDBackNum(0),
    mPassthru(OMX_FALSE) {
  OMX_LOGD("ENTER");
  OMX_U8 mSnd_index = 0;
  for (mSnd_index = 0; mSnd_index < AREN_PASSTHRU_OUTPUT_PORT_NUM; mSnd_index ++) {
    mSndSrvTunnel[mSnd_index] = NULL;
  }
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);

  mVolume.nValue = 100;
  mVolume.nMax = 100;
  mVolume.nMin = 0;
  mSourceControl = NULL;
  mOutputControl = NULL;
  mSourceId = -1;
  mAudioChannel = -1;
  mPhySourceGain = -1;
  mIsSnsConnect = OMX_FALSE;
  mPool = NULL;
  mPushedBdNum = 0;
  mReturnedBdNum = 0;
  mCachedhead = NULL;
  mEOS = OMX_FALSE;
  mMediaHelper = OMX_FALSE;
  mArenAdecConnect = OMX_FALSE;
  mArenClkConnect = OMX_FALSE;
  OMX_LOGD("EXIT");
}

OmxAmpAudioRenderer::~OmxAmpAudioRenderer() {
  OMX_LOGD("%s() line %d", __FUNCTION__, __LINE__);
}

void OmxAmpAudioRenderer::setPassThruMode() {
  OMX_LOGD("set aren passthru");
  mPassthru = OMX_TRUE;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::componentDeInit(OMX_HANDLETYPE hComponent) {
  OMX_LOG_FUNCTION_ENTER;
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxAmpAudioRenderer *aren = static_cast<OmxAmpAudioRenderer *>(comp);
  delete aren;
  reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate = NULL;
  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::getParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  switch (index) {
    case OMX_IndexParamAudioPortFormat: {
      OMX_LOGD("OMX_IndexParamAudioPortFormat");
      OMX_AUDIO_PARAM_PORTFORMATTYPE *audio_param =
          reinterpret_cast<OMX_AUDIO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(audio_param);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("invalid parameter");
        break;
      }
      OmxPortImpl *port = getPort(audio_param->nPortIndex);
      if (NULL == port) {
        OMX_LOGE("invalid port");
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (mInputPort->getPortIndex() == audio_param->nPortIndex) {
        audio_param->eEncoding = OMX_AUDIO_CodingPCM;
      }

      break;
    }

    case OMX_IndexParamAudioPcm: {
      OMX_LOGD("OMX_IndexParamAudioPcm");
      OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
          reinterpret_cast<OMX_AUDIO_PARAM_PCMMODETYPE *>(params);

      // There is only one port.
      if (pcmParams->nPortIndex > 1) {
        return OMX_ErrorUndefined;
      }

      pcmParams->eNumData = OMX_NumericalDataSigned;
      pcmParams->eEndian = OMX_EndianBig;
      pcmParams->bInterleaved = OMX_TRUE;
      pcmParams->nBitPerSample = 16;
      pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
      pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
      pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

      pcmParams->nChannels = mNumChannels;
      pcmParams->nSamplingRate = mSamplingRate;
      break;
    }

    default:
      return OmxComponentImpl::getParameter(index, params);
  }

  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::setParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PCMMODETYPE *pcm_type = NULL;
  OmxPortImpl *port = NULL;

  switch (index) {
    case OMX_IndexParamAudioPortFormat: {
      OMX_AUDIO_PARAM_PORTFORMATTYPE* audio_param =
          reinterpret_cast<OMX_AUDIO_PARAM_PORTFORMATTYPE*>(params);
      err = CheckOmxHeader(audio_param);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("invalid parameter");
        break;
      }
      OmxPortImpl* port = getPort(audio_param->nPortIndex);
      if (NULL == port) {
        OMX_LOGE("invalid port");
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio) {
        port->setAudioParam(audio_param);
        port->updateDomainDefinition();
      }

      break;
    }

    case OMX_IndexParamAudioPcm: {
      OMX_LOGD("OMX_IndexParamAudioPcm");
      pcm_type = reinterpret_cast<OMX_AUDIO_PARAM_PCMMODETYPE *>(params);
      // TODO: workaround for unsupported PCM now, will remove after amp support
      if (pcm_type->nChannels != 2 || pcm_type->nBitPerSample != 16 ||
          pcm_type->eNumData != OMX_NumericalDataSigned) {
        OMX_LOGD("Unsupported PCM type channel %d BitPerSample %d eNumData %d",
            pcm_type->nChannels, pcm_type->nBitPerSample, pcm_type->eNumData);
        postEvent(OMX_EventError, OMX_ErrorUnsupportedSetting, 0);
        return OMX_ErrorNone;
      }
      // There is only one port.
      if (pcm_type->nPortIndex > 1) {
        return OMX_ErrorUndefined;
      }

      port = getPort(pcm_type->nPortIndex);
      if(NULL == port) {
        OMX_LOGE("invalid port");
        err =  OMX_ErrorBadPortIndex;
        break;
      }
      mNumChannels = pcm_type->nChannels;
      mSamplingRate = pcm_type->nSamplingRate;
      mNumData = pcm_type->eNumData;;
      mBitPerSample = pcm_type->nBitPerSample;
      mEndian = pcm_type->eEndian;
      OMX_LOGD("set PCM parameters: mNumChannels = %d, mSamplingRate = %d \
          mNumData %d, mBitPerSample %d, mEndian %d\n",
          mNumChannels, mSamplingRate, mNumData, mBitPerSample, mEndian);

      break;
    }

    default:
      return OmxComponentImpl::setParameter(index, params);
  }

  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::getConfig(
    OMX_INDEXTYPE index, OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  switch(index) {
    case OMX_IndexConfigAudioVolume: {
      OMX_LOGD("OMX_IndexConfigAudioVolume");
      OMX_AUDIO_CONFIG_VOLUMETYPE *volume_conf =
          reinterpret_cast<OMX_AUDIO_CONFIG_VOLUMETYPE *>(config);
      err = CheckOmxHeader(volume_conf);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("invalid config");
        err = OMX_ErrorUndefined;
        break;
      }

      // There is only one port.
      if (volume_conf->nPortIndex > 1) {
        OMX_LOGE("invalid port index");
        err = OMX_ErrorUndefined;
        break;
      }

      volume_conf->bLinear = OMX_TRUE;
      volume_conf->sVolume.nValue = mVolume.nValue;
      volume_conf->sVolume.nMax = mVolume.nMax;
      volume_conf->sVolume.nMin = mVolume.nMin;

      break;
    }

    case OMX_IndexConfigAudioMute: {
      OMX_LOGD("OMX_IndexConfigAudioMute");
      OMX_AUDIO_CONFIG_MUTETYPE b;

      break;
    }

    default:
      return OmxComponentImpl::getConfig(index, config);
  }

  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::setConfig(
    OMX_INDEXTYPE index, const OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  switch(index) {
    case OMX_IndexConfigAudioVolume: {
      OMX_LOGD("OMX_IndexConfigAudioVolume");
      OMX_AUDIO_CONFIG_VOLUMETYPE *volume_conf =
          reinterpret_cast<OMX_AUDIO_CONFIG_VOLUMETYPE *>(config);
      err = CheckOmxHeader(volume_conf);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("invalid config");
        err = OMX_ErrorUndefined;
        break;
      }

      if (mInputPort->getPortIndex() != volume_conf->nPortIndex) {
        OMX_LOGE("bad port index");
        err = OMX_ErrorBadPortIndex;
        break;
      }

      mVolume.nValue = volume_conf->sVolume.nValue;
      mVolume.nMax = volume_conf->sVolume.nMax;
      mVolume.nMin = volume_conf->sVolume.nMin;
      OMX_LOGD("set volume to %d", mVolume.nValue);

      float volumePercent =
        (float)(mVolume.nValue - mVolume.nMin) / (mVolume.nMax -mVolume.nMin);
      UINT32 ampVolume = (UINT32)(volumePercent * 100);

      HRESULT rc = AMP_SND_SetStreamVolume(mSndSrvTunnel[0], ampVolume);
      if (rc != SUCCESS) {
        OMX_LOGE("set volume failed, rc=0x%x", rc);
        err = OMX_ErrorUndefined;
      }

      break;
    }
    case OMX_IndexConfigAudioMute:
      OMX_LOGD("OMX_IndexConfigAudioMute");
      break;
    case OMX_IndexConfigAudioChannelVolume: {
      OMX_LOGD("OMX_IndexConfigAudioChannelVolume");
      OMX_AUDIO_CONFIG_CHANNELVOLUMETYPE *configVolume =
          reinterpret_cast<OMX_AUDIO_CONFIG_CHANNELVOLUMETYPE *>(config);

      err = CheckOmxHeader(configVolume);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("invalid config");
        err = OMX_ErrorUndefined;
        break;
      }

      if (mInputPort->getPortIndex() != configVolume->nPortIndex) {
        OMX_LOGE("bad port index");
        err = OMX_ErrorBadPortIndex;
        break;
      }

      mVolume.nValue = configVolume->sVolume.nValue;
      mVolume.nMax = configVolume->sVolume.nMax;
      mVolume.nMin = configVolume->sVolume.nMin;
      OMX_LOGD("set volume to %d", mVolume.nValue);

      float volumePercent =
        (float)(mVolume.nValue - mVolume.nMin) / (mVolume.nMax -mVolume.nMin);
      UINT32 ampVolume = (UINT32)(volumePercent * 0x7fffffff);

      AMP_APP_PARAMIXGAIN mixerGain;
      HRESULT rc;
      rc = AMP_SND_GetMixerGain(mSndSrvTunnel[0], &mixerGain);
      if (rc != S_OK) {
        OMX_LOGE("failed to get mixer gain, rc=%d", rc);
        mixerGain.uiLeftGain = 0x7fffffff;
        mixerGain.uiRghtGain = 0x7fffffff;
      }
      OMX_LOGD("uiLeftGain=0x%x, uiRightgain=0x%x",
          mixerGain.uiLeftGain, mixerGain.uiRghtGain);

      if (configVolume->nChannel == OMX_AUDIO_ChannelLHS) {
        mixerGain.uiLeftGain = ampVolume;
      } else if (configVolume->nChannel == OMX_AUDIO_ChannelRHS) {
        mixerGain.uiRghtGain = ampVolume;
      } else {
        OMX_LOGW("unsupported channel(%d)", configVolume->nChannel);
      }

      rc = AMP_SND_SetMixerGain(mSndSrvTunnel[0], &mixerGain);
      if (rc != S_OK) {
        OMX_LOGE("failed to set mixer gain, rc=%d", rc);
      }

      break;
    }
    default:
      return OmxComponentImpl::setConfig(index, config);
  }

  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::initRole() {
  OMX_LOG_FUNCTION_ENTER;
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    OMX_LOGE("Not a Marvell component role %s.", mName);
    return OMX_ErrorInvalidComponentName;
  }

  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_AUDIO_RENDERER_PCM, 18)) {
    addRole(OMX_ROLE_AUDIO_RENDERER_PCM);
  } else {
    OMX_LOGE("%s is not a role of OmxAmpAudioRenderer", mName);
    return OMX_ErrorInvalidComponentName;
  }

  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::initPort() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_RENDERER_PCM, 18)) {
    mInputPort = new OmxAmpAudioPort(kAudioPortStartNumber, OMX_DirInput);
    // Set port definition.
    OMX_PARAM_PORTDEFINITIONTYPE def;
    mInputPort->getDefinition(&def);
    def.nBufferSize = 4096 * AMP_AUD_MAX_BUF_NR; // 4k * 8
    def.nBufferCountActual = 4;
    mInputPort->setDefinition(&def);
    // Add port.
    addPort(mInputPort);
  }

  mClockPort = new OmxAmpClockPort(kClockPortStartNumber, OMX_DirInput);
  addPort(mClockPort);

  OMX_LOG_FUNCTION_EXIT;
  return err;
}

void OmxAmpAudioRenderer::onPortClockStart(OMX_BOOL start) {
  if (mSndSrvTunnel[0]) {
    if(start == OMX_TRUE) {
      AMP_SND_StartTunnel(reinterpret_cast<UINT32>(mSndSrvTunnel[0]));
    } else {
      AMP_SND_PauseTunnel(reinterpret_cast<UINT32>(mSndSrvTunnel[0]));
    }
  }
}

OMX_BOOL OmxAmpAudioRenderer::onPortDisconnect(
    OMX_BOOL disconnect, OMX_PTR type) {
  if (disconnect == OMX_FALSE || type == NULL) {
    OMX_LOGE("Wrong parameter disconnect %d type %p", disconnect, type);
    return OMX_FALSE;
  }
  OMX_ERRORTYPE res = OMX_ErrorNone;
  OMX_LOGD("type is %s", type);
  if (kdMemcmp(type, "clk", 3) == 0) {
    connectClockPort(OMX_FALSE);
  } else if (kdMemcmp(type, "adec", 4) == 0) {
    connectADecAudioPort(OMX_FALSE);
  } else {
    OMX_LOGE("The type %s haven't implemented", type);
    return OMX_FALSE;
  }
  return OMX_TRUE;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::connectSoundService() {
  HRESULT err = SUCCESS;

  // Init AMP sound service.
  err = AMP_SND_Init();
  CHECKAMPERRLOG(err, "init amp sound service");

  // Connect ARen's output port and sound service.
  if (mPassthru == OMX_TRUE) {
    for (OMX_U32 app_index = 0; app_index < AREN_PASSTHRU_OUTPUT_PORT_NUM;
        app_index ++) {
      err = AMP_SND_SetupTunnel(mAmpHandle, app_index,
          &mSndSrvTunnel[app_index]);
      CHECKAMPERRLOG(err, "connect ARen and sound service success");
    }
  } else {
    err = AMP_SND_SetupTunnel(mAmpHandle, kAMPPortStartNumber,
        &mSndSrvTunnel[0]);
    CHECKAMPERRLOG(err, "connect ARen and sound service success");
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::connectADecAudioPort(OMX_BOOL connect) {
  HRESULT err = SUCCESS;

  if (connect == OMX_FALSE && mArenAdecConnect == OMX_FALSE) {
    OMX_LOGD("Don't need disconnect aren and adec, already disconnected");
    return OMX_ErrorNone;
  }

  if (connect == OMX_TRUE && mArenAdecConnect == OMX_TRUE) {
    OMX_LOGD("Don't need connect aren and adec, already connected");
    return OMX_ErrorNone;
  }

  if (NULL == mInputPort) {
    OMX_LOGE("The input port is not created.");
    return OMX_ErrorComponentNotFound;
  }
  OMX_HANDLETYPE handle = mInputPort->getTunnelComponent();
  if (NULL == handle) {
    OMX_LOGE("The Tunnel Component is not exist.");
    return OMX_ErrorComponentNotFound;
  }
  AMP_COMPONENT amp_ADec_handle = static_cast<OmxComponentImpl *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(handle)->pComponentPrivate)->getAmpHandle();
  if (NULL == amp_ADec_handle) {
    OMX_LOGE("Get tunnel amp component NULL.");
    return OMX_ErrorComponentNotFound;
  }
  if (mPassthru == OMX_TRUE) {
    // query information about ADEC component
    AMP_COMPONENT_INFO adec_cmp_info;
    kdMemset(&adec_cmp_info, 0, sizeof(adec_cmp_info));
    AMP_PORT_INFO adec_port_info;
    kdMemset(&adec_port_info, 0, sizeof(adec_port_info));
    AMP_RPC(err, AMP_ADEC_QueryInfo, amp_ADec_handle, &adec_cmp_info);
    CHECKAMPERRLOG(err, "AMP_ADEC_QueryInfo()");

    // query information about AREN component
    AMP_COMPONENT_INFO aren_cmp_info;
    kdMemset(&aren_cmp_info, 0, sizeof(aren_cmp_info));
    AMP_PORT_INFO aren_port_info;
    kdMemset(&aren_port_info, 0, sizeof(aren_port_info));
    AMP_RPC(err, AMP_AREN_QueryInfo, mAmpHandle, &aren_cmp_info);
    CHECKAMPERRLOG(err, "AMP_AREN_QueryInfo()");

    // connect adec and aren
    for (OMX_S32 adec_index = 0; adec_index < adec_cmp_info.uiOutputPortNum;
        adec_index++) {
      AMP_RPC(err, AMP_ADEC_QueryPort, amp_ADec_handle, AMP_PORT_OUTPUT,
        adec_index, &adec_port_info);
      CHECKAMPERRLOG(err, "AMP_ADEC_QueryPort()");
      for (OMX_S32 aren_index = 0; aren_index < aren_cmp_info.uiInputPortNum;
          aren_index++) {
        AMP_RPC(err, AMP_AREN_QueryPort, mAmpHandle, AMP_PORT_INPUT, aren_index,
            &aren_port_info);
        CHECKAMPERRLOG(err, "AMP_AREN_QueryPort()");
        if ((adec_port_info.ePortType == AMP_PORT_ADEC_OUT_PCM &&
            aren_port_info.ePortType == AMP_PORT_AREN_IN_PCM) ||
            ((adec_port_info.ePortType == AMP_PORT_ADEC_OUT_SPDIF &&
            aren_port_info.ePortType == AMP_PORT_AREN_IN_SPDIF)) ||
            ((adec_port_info.ePortType == AMP_PORT_ADEC_OUT_HDMI &&
            aren_port_info.ePortType == AMP_PORT_AREN_IN_HDMI))) {
          if (connect == OMX_TRUE) {
            err = AMP_ConnectComp(amp_ADec_handle, adec_index,
                mAmpHandle, aren_index);
            CHECKAMPERRLOG(err, \
                "connect component ADec port and ARen");
          } else {
            err = AMP_DisconnectComp(amp_ADec_handle, adec_index,
                mAmpHandle, aren_index);
            CHECKAMPERRLOG(err, \
                "disconnect component ADec port and ARen");
          }
        }
      }
    }
  } else {
    if (connect == OMX_TRUE) {
      err = AMP_ConnectComp(amp_ADec_handle, kAMPPortStartNumber,
          mAmpHandle, kAMPPortStartNumber + 1);
      CHECKAMPERRLOG(err, "connect component ADec and ARen");
    } else {
      err = AMP_DisconnectComp(amp_ADec_handle, kAMPPortStartNumber,
          mAmpHandle, kAMPPortStartNumber + 1);
      CHECKAMPERRLOG(err, "disconnect component ADec and ARen");
    }
  }
  if (connect == OMX_TRUE) {
    mArenAdecConnect = OMX_TRUE;
  } else {
    mArenAdecConnect = OMX_FALSE;
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::connectClockPort(OMX_BOOL connect) {
  HRESULT err = SUCCESS;
  AMP_AREN_PARAST ArenParaSt;
  OMX_HANDLETYPE handle = NULL;


  if (connect == OMX_FALSE && mArenClkConnect == OMX_FALSE) {
    OMX_LOGD("Don't need disconnect aren and clk, already disconnected");
    return OMX_ErrorNone;
  }

  if (connect == OMX_TRUE && mArenClkConnect == OMX_TRUE) {
    OMX_LOGD("Don't need connect aren and clk, already connected");
    return OMX_ErrorNone;
  }

  if (NULL == mClockPort) {
    OMX_LOGE("The clock port is not created.");
    return OMX_ErrorComponentNotFound;
  }

  handle = mClockPort->getTunnelComponent();
  if (NULL == handle) {
    OMX_LOGE("The Tunnel Component is not exist.");
    return OMX_ErrorComponentNotFound;
  }
  AMP_COMPONENT amp_clock_handle = static_cast<OmxComponentImpl *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(handle)->pComponentPrivate)->getAmpHandle();
  if (NULL == amp_clock_handle) {
    OMX_LOGE("Get tunnel amp component NULL.");
    return OMX_ErrorComponentNotFound;
  }

  if (mPassthru == OMX_TRUE) {
    AMP_COMPONENT_INFO aren_cmp_info;
    kdMemset(&aren_cmp_info, 0, sizeof(aren_cmp_info));
    AMP_PORT_INFO aren_port_info;
    kdMemset(&aren_port_info, 0, sizeof(aren_port_info));
    AMP_RPC(err, AMP_AREN_QueryInfo, mAmpHandle, &aren_cmp_info);
    CHECKAMPERRLOG(err, "AMP_AREN_QueryInfo()");

    OMX_U32 clk_output_idx = 0;
    for (OMX_S32 aren_index = 0; aren_index < AREN_PASSTHRU_OUTPUT_PORT_NUM;
        aren_index++) {
      AMP_RPC(err, AMP_AREN_QueryPort, mAmpHandle, AMP_PORT_INPUT, aren_index,
          &aren_port_info);
      CHECKAMPERRLOG(err, "AMP_AREN_QueryPort()");

      if (aren_port_info.ePortType != AMP_PORT_AREN_IN_CLOCK) {
        OMX_LOGD("Ignore AREN input port %d ePortType:%d",
            aren_index, aren_port_info.ePortType);
        continue;
      }
      if (connect == OMX_TRUE) {
        err = AMP_ConnectComp(amp_clock_handle, clk_output_idx,
            mAmpHandle, aren_index);
        CHECKAMPERRLOG(err, "connect CLK & AREN");
      } else {
        err = AMP_DisconnectComp(amp_clock_handle, clk_output_idx,
            mAmpHandle, aren_index);
        CHECKAMPERRLOG(err, "disconnect CLK & AREN");
      }
      if (connect == OMX_TRUE) {
        // Associate ARen's data input port to clock port.
        kdMemset(&ArenParaSt, 0x00, sizeof(AMP_AREN_PARAST));
        ArenParaSt._d = AMP_AREN_PARAIDX_PORTASSOCCLK;
        ArenParaSt._u.PORTASSOCCLK.uiAssocIdx = clk_output_idx;
        AMP_RPC(err, AMP_AREN_SetPortParameter, mAmpHandle, AMP_PORT_INPUT,
            AREN_PASSTHRU_OUTPUT_PORT_NUM + aren_index,
            AMP_AREN_PARAIDX_PORTASSOCCLK, &ArenParaSt);
        CHECKAMPERRLOG(err,
            "set AREN port parameter AMP_AREN_PARAIDX_PORTASSOCCLK");
      }
      clk_output_idx ++;
    }
  } else {
    if (connect == OMX_TRUE) {
      // ARen connect to AMP clock's first port
      err = AMP_ConnectComp(amp_clock_handle, kAMPPortStartNumber + 1,
          mAmpHandle, kAMPPortStartNumber);
      CHECKAMPERRLOG(err, "connect CLK & AREN");

      // Associate ARen's data input port to clock port.
      kdMemset(&ArenParaSt, 0x00, sizeof(AMP_AREN_PARAST));
      ArenParaSt._d = AMP_AREN_PARAIDX_PORTASSOCCLK;
      ArenParaSt._u.PORTASSOCCLK.uiAssocIdx = 0;
      AMP_RPC(err, AMP_AREN_SetPortParameter, mAmpHandle, AMP_PORT_INPUT, 1,
              AMP_AREN_PARAIDX_PORTASSOCCLK, &ArenParaSt);
      CHECKAMPERRLOG(err,
          "set AREN port parameter AMP_AREN_PARAIDX_PORTASSOCCLK");
    } else {
      err = AMP_DisconnectComp(amp_clock_handle, kAMPPortStartNumber + 1,
          mAmpHandle, kAMPPortStartNumber);
      CHECKAMPERRLOG(err, "disconnect CLK & AREN");
    }
  }
  if (connect == OMX_TRUE) {
    mArenClkConnect = OMX_TRUE;
  } else {
    mArenClkConnect = OMX_FALSE;
  }
  return OMX_ErrorNone;
}

HRESULT OmxAmpAudioRenderer::registerEvent(AMP_EVENT_CODE event) {
  HRESULT err = SUCCESS;
  if (mListener) {
    err = AMP_Event_RegisterCallback(mListener, event, eventHandle,
        static_cast<void *>(this));
    CHECKAMPERRLOG(err, "register AREN notify");

    if (!err) {
      AMP_RPC(err, AMP_AREN_RegisterNotify, mAmpHandle,
          AMP_Event_GetServiceID(mListener), event);
      CHECKAMPERRLOG(err, "register AREN callback");
    }
  } else {
    OMX_LOGE("mListener is not created yet.");
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

HRESULT OmxAmpAudioRenderer::unRegisterEvent(AMP_EVENT_CODE event) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mListener) {
    AMP_RPC(err, AMP_AREN_UnregisterNotify, mAmpHandle,
        AMP_Event_GetServiceID(mListener), event);
    CHECKAMPERRLOG(err, "unregister AREN notify");

    if (!err) {
      err = AMP_Event_UnregisterCallback(mListener, event, eventHandle);
      CHECKAMPERRLOG(err, "unregister AREN callback");
    }
  } else {
    OMX_LOGE("mListener is not created yet.");
  }
  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::prepare() {
  OMX_LOG_FUNCTION_ENTER;
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
  OMX_ERRORTYPE res = OMX_ErrorNone;
  AMP_COMPONENT_CONFIG mConfig;
  AMP_COMPONENT_INFO cmp_info;

  initAQ();

  // Get AMP factory.
  err = AMP_GetFactory(&mFactory);
  CHECKAMPERRLOG(err, "get AMP factory");

  // Create AMP ARen component.
  AMP_RPC(err, AMP_FACTORY_CreateComponent, mFactory, AMP_COMPONENT_AREN,
      1, &mAmpHandle);
  CHECKAMPERRLOG(err, "create AREN component");

  // Set configs for ARen.
  kdMemset(&mConfig, 0x00, sizeof(AMP_COMPONENT_CONFIG));
  mConfig._d = AMP_COMPONENT_AREN;
  if (mPassthru == OMX_TRUE && mInputPort->isTunneled()) {
    OMX_LOGD("aren is passthru and tunneled");
    // if passthru mode, open three clock port and three input and three output port
    mConfig._u.pAREN.uiInClkPortNum = AREN_PASSTHRU_OUTPUT_PORT_NUM;
    mConfig._u.pAREN.uiInPcmPortNum  = 1;
    mConfig._u.pAREN.uiInSpdifPortNum  = 1;
    mConfig._u.pAREN.uiInHdmiPortNum  = 1;
    mConfig._u.pAREN.uiOutPcmPortNum = 1;
    mConfig._u.pAREN.uiOutSpdifPortNum = 1;
    mConfig._u.pAREN.uiOutHdmiPortNum = 1;
  } else {
    mConfig._u.pAREN.uiInClkPortNum = 1;
    mConfig._u.pAREN.uiInPcmPortNum = 1;
    mConfig._u.pAREN.uiOutPcmPortNum = 1;
  }

  if (mInputPort->isTunneled()) {
    mConfig._u.pAREN.mode = AMP_TUNNEL;
  } else {
    mConfig._u.pAREN.mode = AMP_NON_TUNNEL;
#if 1
    OMX_LOGD("AREN create cbuf");
    OMX_U32 ret = AMP_CBUFCreate(stream_in_buf_size, 0, stream_in_bd_num,
        stream_in_align_size, true, &mPool, true, 64 * 1024, AMP_SHM_STATIC, true);
    if (AMPCBUF_SUCCESS != ret) {
      OMX_LOGW("AREN Cbuf Create Error %u.", ret);
      // create cbuf fail, use omx bd
      OMX_PARAM_PORTDEFINITIONTYPE def;
      mInputPort->getDefinition(&def);
      def.nBufferSize = 4096 * AMP_AUD_MAX_BUF_NR; // 4k * 8
      def.nBufferCountActual = 64;
      mInputPort->setDefinition(&def);
    }
#else
    OMX_PARAM_PORTDEFINITIONTYPE def;
    mInputPort->getDefinition(&def);
    def.nBufferSize = 4096 * AMP_AUD_MAX_BUF_NR; // 4k * 8
    def.nBufferCountActual = 64;
    mInputPort->setDefinition(&def);
#endif
  }
  // Open ARen with configs.
  AMP_RPC(err, AMP_AREN_Open, mAmpHandle, &mConfig);
  CHECKAMPERRLOG(err, "open AREN handle");

  AMP_RPC(err, AMP_AREN_SetState, mAmpHandle, AMP_IDLE);
  CHECKAMPERRLOG(err, "set AREN state to idle");

  kdMemset(&cmp_info, 0x00, sizeof(AMP_COMPONENT_INFO));
  AMP_RPC(err, AMP_AREN_QueryInfo, mAmpHandle, &cmp_info);
  CHECKAMPERRLOG(err, "query AREN's info");

  OMX_LOGV("cmp_info.uiInputPortNum = %d, cmp_info.uiOutputPortNum = %d",
      cmp_info.uiInputPortNum, cmp_info.uiOutputPortNum);

  // Connect ARen's clock port.
  if (mClockPort && mClockPort->isTunneled()) {
    res = connectClockPort(OMX_TRUE);
    if (OMX_ErrorNone != res) {
      OMX_LOGE("Connect clock port failed 0x%x", res);
      goto FAILED_EXIT;
    }
  }

  // Connect ARen's input port.
  if (mInputPort->isTunneled()) {
    // Tunnel mode.
    res = connectADecAudioPort(OMX_TRUE);
    if (OMX_ErrorNone != res) {
      OMX_LOGE("Connect ADec output port failed 0x%x", res);
      goto FAILED_EXIT;
    }
  } else {
    // Non-Tunnel mode.
    err = AMP_ConnectApp(mAmpHandle, AMP_PORT_INPUT,
        1, pushBufferDone, static_cast<void *>(this));
    CHECKAMPERRLOG(err, "connect with AREN input port");
  }

  res = connectSoundService();
  if (OMX_ErrorNone != res) {
    OMX_LOGE("Connect sound service failed 0x%x", res);
    mIsSnsConnect = OMX_FALSE;
    return res;
  } else {
    mIsSnsConnect = OMX_TRUE;
    // Set Source gain
    setMixGain();
  }

  // Register a callback so that it can receive event - for example EOS -from ARen.
  mListener = AMP_Event_CreateListener(AMP_EVENT_TYPE_MAX, 0);
  if (mListener) {
    registerEvent(AMP_EVENT_API_AREN_CALLBACK);
  }

FAILED_EXIT:
  OMX_LOG_FUNCTION_EXIT;
  return res;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::preroll() {
  OMX_LOGD("%s() line %d", __FUNCTION__, __LINE__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::start() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (!mInputPort->isTunneled()) {
    err = registerBds(static_cast<OmxAmpAudioPort *>(mInputPort));
    CHECKAMPERRLOG(err, "register BDs in AREN input port");
  }

  err = setAmpState(AMP_EXECUTING);

  // Create data thread only for non-tunnel mode, because in tunnel mode there is no data flow.
  if (!mInputPort->isTunneled()) {
    OMX_LOGD("Create data thread");
    mPauseLock = kdThreadMutexCreate(KD_NULL);
    mBufferLock = kdThreadMutexCreate(KD_NULL);
    mPauseCond = kdThreadCondCreate(KD_NULL);
    mThread = kdThreadCreate(mThreadAttr, threadEntry,(void *)this);
  }

  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::pause() {
  OMX_LOG_FUNCTION_ENTER;

  OMX_ERRORTYPE err = OMX_ErrorNone;
  // Pause data thread firstly.
  if (mThread) {
    mPaused = OMX_TRUE;
    mShouldExit = OMX_TRUE;
  }
  // pause sound service to pause output
  AMP_SND_PauseTunnel(reinterpret_cast<UINT32>(mSndSrvTunnel[0]));
  // Pause AMP ARen.
  err = setAmpState(AMP_PAUSED);

  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::resume() {
  OMX_LOG_FUNCTION_ENTER;

  OMX_ERRORTYPE err = OMX_ErrorNone;
  // Resume AMP ARen firstly.
  err = setAmpState(AMP_EXECUTING);
  // start sound service to start output
  AMP_SND_StartTunnel(reinterpret_cast<UINT32>(mSndSrvTunnel[0]));

  // Resume data thread.
  if (mThread) {
    if (mInputPort->getCachedBuffer() != NULL) {
      returnBuffer(mInputPort, mInputPort->getCachedBuffer());
    }
    mShouldExit = OMX_FALSE;
    kdThreadMutexLock(mPauseLock);
    mPaused = OMX_FALSE;
    kdThreadCondSignal(mPauseCond);
    kdThreadMutexUnlock(mPauseLock);
  }

  OMX_LOG_FUNCTION_EXIT;
  return err;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::stop() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  HRESULT result = SUCCESS;

  mShouldExit = OMX_TRUE;
  if (mPauseLock) {
    kdThreadMutexLock(mPauseLock);
    mPaused = OMX_FALSE;
    kdThreadCondSignal(mPauseCond);
    kdThreadMutexUnlock(mPauseLock);
  }
  if (mThread) {
    void *retval;
    KDint ret;
    ret = kdThreadJoin(mThread, &retval);
    OMX_LOGD("%s() line %d, now thread has been joined", __FUNCTION__, __LINE__);
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
  // stop sound service to stop output
  AMP_SND_StopTunnel(reinterpret_cast<UINT32>(mSndSrvTunnel[0]));
  // Set State.
  AMP_RPC(result, AMP_AREN_SetState, mAmpHandle, AMP_IDLE);
  if (result != SUCCESS) {
    OMX_LOGE("AMP_AREN_SetState(AMP_IDLE) failed with error 0x%x", result);
  }
  OMX_LOGD("AMP state changed to AMP_IDLE", AMP_IDLE);

  if (!mInputPort->isTunneled()) {
    // Waiting for input/output buffers back
    OMX_U32 wait_count = 100;
    while ((mInputFrameNum > mInBDBackNum) && wait_count > 0) {
      usleep(5000);
      wait_count--;
    }
    OMX_LOGD("inputframe %d inbackbd %d wait %d\n",
      mInputFrameNum, mInBDBackNum, wait_count);

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

    err = unregisterBds(static_cast<OmxAmpAudioPort *>(mInputPort));
    CHECKAMPERRLOG(err, "unregister BDs in input port");
  }

  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::release() {
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

  HRESULT result = SUCCESS;
  OMX_ERRORTYPE res = OMX_ErrorNone;

  if (mListener) {
    unRegisterEvent(AMP_EVENT_API_AREN_CALLBACK);
    AMP_Event_DestroyListener(mListener);
    mListener = NULL;
  }

  // Disconnect ARen's input port.
  if (!mInputPort->isTunneled()) {
    result = AMP_DisconnectApp(mAmpHandle, AMP_PORT_INPUT, 1, pushBufferDone);
    CHECKAMPERRLOG(result, "disconnect AREN's input port");
  } else {
#ifdef DISCONNECT_AREN_ADEC
    // Tunnel mode, disconnect aren adec
    /* As  adec already destoryed, but getAmpHandle still get not null
    * It would cause crash in GTV, disable it first.
    */
    // TODO: check why getAmpHandle return not correct
    res = connectADecAudioPort(OMX_FALSE);
    OMX_LOGD("Disconnect ADec output port return 0x%x", res);
#endif
  }

#ifdef DISCONNECT_AREN_CLOCK
  if (mClockPort && mClockPort->isTunneled()) {
    // Disconnect ARen's clock port.
    res = connectClockPort(OMX_FALSE);
    OMX_LOGD("Disconnect clock port return 0x%x", res);
  }
#endif
  mArenAdecConnect = OMX_FALSE;
  mArenClkConnect = OMX_FALSE;

  // Disconnect ARen from sound service.
  if (mPassthru == OMX_TRUE) {
    for (OMX_U32 app_index = 0; app_index < AREN_PASSTHRU_OUTPUT_PORT_NUM;
        app_index++) {
      result = AMP_SND_RemoveTunnel(mSndSrvTunnel[app_index]);
      CHECKAMPERRLOG(result, "AMP_SND_RemoveTunnel()");
      mSndSrvTunnel[app_index] = NULL;
    }
  } else {
    result = AMP_SND_RemoveTunnel(mSndSrvTunnel[0]);
    CHECKAMPERRLOG(result, "AMP_SND_RemoveTunnel()");
    mSndSrvTunnel[0] = NULL;
  }
  result = AMP_SND_Deinit();
  CHECKAMPERRLOG(result, "AREN disconnected from sound service");

  // Close.
  AMP_RPC(result, AMP_AREN_Close, mAmpHandle);
  CHECKAMPERRLOG(result, "AMP_AREN_Close()");

  // Destory.
  AMP_RPC(result, AMP_AREN_Destroy, mAmpHandle);
  CHECKAMPERRLOG(result, "AMP_AREN_Destroy()");


  if (mOutputControl != NULL) {
    mOutputControl->removeObserver(mObserver);
  }
  if ((mSourceControl != NULL) && (-1 != mSourceId)) {
    mSourceControl->exitSource(mSourceId);
  }

  AMP_FACTORY_Release(mAmpHandle);
  mAmpHandle = NULL;

  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::flush() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;

  // clear dumped data
  if (CheckMediaHelperEnable(mMediaHelper, CLEAR_DUMP) == OMX_TRUE) {
    mediainfo_remove_dumped_data(MEDIA_AUDIO);
  }
  kdThreadMutexLock(mBufferLock);
  OMX_U32 wait_count = 100;
  if (!mInputPort->isEmpty()) {
    OMX_LOGE("The buffer queue have not been cleared.");
  }
  if (NULL != mCachedhead) {
    OMX_LOGD("return the cached buffer header.");
    returnBuffer(mInputPort, mCachedhead);
    mCachedhead = NULL;
  }
  // stop sound service to stop output
  AMP_SND_StopTunnel(reinterpret_cast<UINT32>(mSndSrvTunnel[0]));
  // Set ARen to AMP_IDLE so that it can return all buffers.
  setAmpState(AMP_IDLE);

  AMP_RPC(err, AMP_AREN_ClearPortBuf, mAmpHandle, AMP_PORT_INPUT, 0);
  CHECKAMPERRLOGNOTRETURN(err, "Clear AREN PortBuf");

  // Waiting for input/output buffer back
  if (!mInputPort->isTunneled()) {
    wait_count = 100;
    while ((mInputFrameNum > mInBDBackNum) && wait_count > 0) {
      usleep(5000);
      wait_count--;
    }
    OMX_LOGD("inputframe %d inbackbd %d wait %d\n",
      mInputFrameNum, mInBDBackNum, wait_count);
    returnCachedBuffers(mInputPort);
    mInputFrameNum = 0;
    mInBDBackNum = 0;
  }
  if (NULL != mPool) {
    wait_count = 200;
    while (mPushedBdNum > mReturnedBdNum && wait_count > 0) {
      usleep(5000);
      wait_count--;
    }
    if (!wait_count) {
      OMX_LOGE("There are %d pushed Bd not returned in %d back %d.",
          mPushedBdNum - mReturnedBdNum, mPushedBdNum, mReturnedBdNum);
    }
    if (AMPCBUF_SUCCESS != AMP_CBUFReset(mPool)) {
      OMX_LOGE("Cbuf AMP_CBUFReSet error.");
    }
    mPushedBdNum = 0;
    mReturnedBdNum = 0;
  }
  if (getState() == OMX_StateExecuting) {
    AMP_RPC(err, AMP_AREN_SetState, mAmpHandle, AMP_EXECUTING);
    CHECKAMPERRLOGNOTRETURN(err, "set AREN state to executing");
  } else if (getState() == OMX_StatePause) {
    AMP_RPC(err, AMP_AREN_SetState, mAmpHandle, AMP_PAUSED);
    CHECKAMPERRLOGNOTRETURN(err, "set AREN state to pause");
  }
  mStreamPosition = 0;
  mEOS = OMX_FALSE;
  kdThreadMutexUnlock(mBufferLock);
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpAudioRenderer::pushAmpBd(AMP_PORT_IO port,
    UINT32 portindex, AMP_BD_ST *bd) {
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_AREN_PushBD, mAmpHandle, port, portindex, bd);
  CHECKAMPERRLOG(err, "push BD to AREN");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpAudioRenderer::registerBds(OmxAmpAudioPort *port) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  UINT32 uiPortIdx = 0;
  AMP_BD_ST *bd;

  for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
    AmpBuffer *amp_buffer = port->getAmpBuffer(i);
    if (NULL != amp_buffer) {
      amp_buffer->addAudioFrameInfoTag(mNumChannels, mBitPerSample,
          mSamplingRate);
      amp_buffer->addAVSPtsTag();
      AMP_RPC(err, AMP_AREN_RegisterBD, mAmpHandle, AMP_PORT_INPUT,
          uiPortIdx, amp_buffer->getBD());
      CHECKAMPERRLOG(err, "register BD at AREN input port");
    }
  }

  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpAudioRenderer::unregisterBds(OmxAmpAudioPort *port) {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  AMP_BD_ST *bd;
  UINT32 uiPortIdx = 0;

  for (OMX_U32 i = 0; i < port->getBufferCount(); i++) {
    bd = port->getBD(i);
    if (NULL != bd) {
      AMP_RPC(err, AMP_AREN_UnregisterBD, mAmpHandle, AMP_PORT_INPUT,
          uiPortIdx, bd);
      CHECKAMPERRLOG(err, "unregsiter BDs at AREN input port");
    }
  }

  OMX_LOG_FUNCTION_EXIT;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::setAmpState(AMP_STATE state) {
  OMX_LOGD("state = %s(%d)", AmpState2String(state), state);
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_AREN_SetState, mAmpHandle, state);
  CHECKAMPERRLOG(err, "set AREN state");
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpAudioRenderer::getAmpState(AMP_STATE *state) {
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_AREN_GetState, mAmpHandle, state);
  CHECKAMPERRLOG(err, "get AREN state");
  OMX_LOGD("state = %s(%d)", AmpState2String(*state), *state);
  return static_cast<OMX_ERRORTYPE>(err);
}


/**
 * @brief: Push bd to Cbuf
 */
UINT32 OmxAmpAudioRenderer::pushCbufBd(OMX_BUFFERHEADERTYPE *in_head) {
  UINT32 ret, offset;
  OMX_U32 flag = (in_head->nFlags & OMX_BUFFERFLAG_EOS) ?
                AMP_MEMINFO_FLAG_EOS_MASK : 0;
  ret = AMP_CBUFWriteData(mPool, in_head->pBuffer + in_head->nOffset,
      in_head->nFilledLen, flag, &offset);
  if (AMPCBUF_SUCCESS == ret) {
    mInputFrameNum ++;
    if (CheckMediaHelperEnable(mMediaHelper,
        PRINT_BD_IN) == OMX_TRUE) {
      OMX_LOGD("<In> %u/%u, size = %d, pts = "TIME_FORMAT
          ", flag = 0x%x, total len = %d",
          mInBDBackNum, mInputFrameNum, in_head->nFilledLen,
          TIME_ARGS(in_head->nTimeStamp), in_head->nFlags,
          mStreamPosition);
    }
    AMP_BDTAG_AVS_PTS pts_tag;
    kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_AVS_PTS));
    pts_tag.Header = {AMP_BDTAG_SYNC_PTS_META, sizeof(AMP_BDTAG_AVS_PTS)};
    convertUsto90K(in_head->nTimeStamp, &pts_tag.uPtsHigh, &pts_tag.uPtsLow);
    AMP_CBUFInsertTag(mPool, reinterpret_cast<UINT8 *>(&pts_tag), offset);
    AMP_SHM_HANDLE mShm;
    AMP_CBUFGetShm(mPool, &mShm);
    AMP_BDTAG_AUD_FRAME_INFO audio_frame_tag;
    kdMemset(&audio_frame_tag, 0, sizeof(AMP_BDTAG_AUD_FRAME_INFO));
    audio_frame_tag.Header = {AMP_BDTAG_AUD_FRAME_CTRL,
        sizeof(AMP_BDTAG_AUD_FRAME_INFO)};
    audio_frame_tag.uChanNr = mNumChannels;
    audio_frame_tag.uBitDepth = mBitPerSample;
    audio_frame_tag.uSampleRate = mSamplingRate;
    audio_frame_tag.uMemHandle = mShm;
    audio_frame_tag.uDataLength = in_head->nFilledLen;
    audio_frame_tag.uSize = in_head->nFilledLen;
    switch(mNumChannels) {
      case 1:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_CNTR;
        break;
      case 2:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        break;
      case 3:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        audio_frame_tag.uDataMask[2] = AMP_AUDIO_CHMASK_CNTR;
        break;
      case 4:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        audio_frame_tag.uDataMask[2] = AMP_AUDIO_CHMASK_CNTR;
        audio_frame_tag.uDataMask[3] = AMP_AUDIO_CHMASK_LFE;
        break;
      case 5:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        audio_frame_tag.uDataMask[2] = AMP_AUDIO_CHMASK_CNTR;
        audio_frame_tag.uDataMask[3] = AMP_AUDIO_CHMASK_SRRD_LEFT;
        audio_frame_tag.uDataMask[4] = AMP_AUDIO_CHMASK_SRRD_RGHT;
        break;
      case 6:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        audio_frame_tag.uDataMask[2] = AMP_AUDIO_CHMASK_CNTR;
        audio_frame_tag.uDataMask[3] = AMP_AUDIO_CHMASK_LFE;
        audio_frame_tag.uDataMask[4] = AMP_AUDIO_CHMASK_SRRD_LEFT;
        audio_frame_tag.uDataMask[5] = AMP_AUDIO_CHMASK_SRRD_RGHT;
        break;
      case 7:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        audio_frame_tag.uDataMask[2] = AMP_AUDIO_CHMASK_CNTR;
        audio_frame_tag.uDataMask[3] = AMP_AUDIO_CHMASK_SRRD_LEFT;
        audio_frame_tag.uDataMask[4] = AMP_AUDIO_CHMASK_SRRD_RGHT;
        audio_frame_tag.uDataMask[5] = AMP_AUDIO_CHMASK_LEFT_REAR;
        audio_frame_tag.uDataMask[6] = AMP_AUDIO_CHMASK_RGHT_REAR;
        break;
      case 8:
        audio_frame_tag.uDataMask[0] = AMP_AUDIO_CHMASK_LEFT;
        audio_frame_tag.uDataMask[1] = AMP_AUDIO_CHMASK_RGHT;
        audio_frame_tag.uDataMask[2] = AMP_AUDIO_CHMASK_CNTR;
        audio_frame_tag.uDataMask[3] = AMP_AUDIO_CHMASK_LFE;
        audio_frame_tag.uDataMask[4] = AMP_AUDIO_CHMASK_SRRD_LEFT;
        audio_frame_tag.uDataMask[5] = AMP_AUDIO_CHMASK_SRRD_RGHT;
        audio_frame_tag.uDataMask[6] = AMP_AUDIO_CHMASK_LEFT_REAR;
        audio_frame_tag.uDataMask[7] = AMP_AUDIO_CHMASK_RGHT_REAR;
        break;
      default:
        break;
    }
    if (in_head->nFlags & OMX_BUFFERFLAG_EOS) {
      audio_frame_tag.uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
      OMX_LOGD("AREN meet EOS frame");
    }
    audio_frame_tag.uDataLength = in_head->nFilledLen;
    audio_frame_tag.uMemOffset[0] = offset;
    AMP_CBUFInsertTag(mPool, reinterpret_cast<UINT8 *>(
        &audio_frame_tag), offset);
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
UINT32 OmxAmpAudioRenderer::requestCbufBd() {
  AMP_BD_ST *bd;
  UINT32 ret, offset, framesize;
  ret = AMP_CBUFRequest(mPool, &bd, &offset, &framesize);
  if (AMPCBUF_SUCCESS == ret) {
    pushAmpBd(AMP_PORT_INPUT, kAMPPortStartNumber + 1, bd);
    mPushedBdNum++;
    if (CheckMediaHelperEnable(mMediaHelper, PRINT_BD_IN) == OMX_TRUE) {
      OMX_LOGD("Cbuf <%u/%u> Push Bd to Amp: offset %u  size %u", mReturnedBdNum,
          mPushedBdNum, offset, framesize);
    }
    // dump cbuf es data
    if (CheckMediaHelperEnable(mMediaHelper,
        DUMP_AUDIO_CBUF_ES) == OMX_TRUE) {
      AMP_SHM_HANDLE mshm;
      AMP_CBUFGetShm(mPool, &mshm);
      void *pVirAddr = NULL;
      AMP_SHM_GetVirtualAddress(mshm, 0, &pVirAddr);
      mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_ES,
          reinterpret_cast<OMX_U8 *>(pVirAddr) + offset, framesize);
    }
  } else if (AMPCBUF_LACKBD == ret) {
    OMX_LOGV("Cbuf Lack of Bd, waitting.");
  } else if (AMPCBUF_LACKDATA == ret) {
    OMX_LOGV("Cbuf Lack of Data, waitting.");
  } else {
    OMX_LOGD("Cbuf Request Bd err %d.", ret);
  }
  return ret;
}

void OmxAmpAudioRenderer::pushBufferLoop() {
  OMX_LOG_FUNCTION_ENTER;
  AMP_BD_ST *in_buf = NULL, *out_buf = NULL;
  OMX_BUFFERHEADERTYPE *in_head = NULL, *out_head = NULL;
  OMX_BOOL isCaching = OMX_FALSE;
Loop_start:
  while (!mShouldExit) {
    while (!mShouldExit && (!mInputPort->isEmpty() || mCachedhead != NULL)) {
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
        if (!isCaching && in_head->nFlags & OMX_BUFFERFLAG_EOS) {
          OMX_LOGD("Meet EOS Frame, wait for stop");
          mEOS = OMX_TRUE;
        }
        AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
            in_head->pPlatformPrivate);
        if (amp_buffer) {
          if (isCaching != OMX_TRUE && mEndian == 0) {
            OMX_U8 convert_endian;
            OMX_U32 convert_index = 0;
            OMX_U8 *pData = in_head->pBuffer + in_head->nOffset;
            for (; convert_index < in_head->nFilledLen; convert_index += 2) {
              convert_endian = pData[convert_index];
              pData[convert_index] = pData[convert_index + 1];
              pData[convert_index + 1] = convert_endian;
            }
          }
          // dump es data
          if (isCaching != OMX_TRUE && CheckMediaHelperEnable(mMediaHelper,
              DUMP_AUDIO_ES) == OMX_TRUE) {
            mediainfo_dump_data(MEDIA_AUDIO, MEDIA_DUMP_ES,
                in_head->pBuffer + in_head->nOffset, in_head->nFilledLen);
          }
          if (mPool != NULL) {
            UINT32 ret;
            ret = requestCbufBd();
            if (AMPCBUF_LACKBD == ret) {
              OMX_LOGV("Cbuf Lack of Bd, waitting.");
              kdThreadMutexUnlock(mBufferLock);
              usleep(kDefaultLoopSleepTime);
              continue;
            }
            ret = pushCbufBd(in_head);
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
            // Push BD to AMP.
            amp_buffer->updateAVSPts(in_head, mStreamPosition);
            amp_buffer->updateAudioFrameInfo(in_head);
            mInputFrameNum++;
            mStreamPosition += in_head->nFilledLen;
            if (CheckMediaHelperEnable(mMediaHelper,
                PRINT_BD_IN) == OMX_TRUE) {
              OMX_LOGD("<In>, size = %d, pts = "TIME_FORMAT ", flag = 0x%x, total len = %d",
                  in_head->nFilledLen, TIME_ARGS(in_head->nTimeStamp), in_head->nFlags,
                  mStreamPosition);
            }
            pushAmpBd(AMP_PORT_INPUT, kAMPPortStartNumber + 1, amp_buffer->getBD());
          }
        }
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

  OMX_LOG_FUNCTION_EXIT;
  return;
}

void* OmxAmpAudioRenderer::threadEntry(void * args) {
  OmxAmpAudioRenderer *arender = (OmxAmpAudioRenderer *)args;
  prctl(PR_SET_NAME, "OmxAudioRender", 0, 0, 0);
  if (arender) {
    arender->pushBufferLoop();
  }
  return NULL;
}

HRESULT OmxAmpAudioRenderer::pushBufferDone(
    AMP_COMPONENT component,
    AMP_PORT_IO port_Io,
    UINT32 port_idx,
    AMP_BD_ST *bd, void *context) {
  HRESULT result;
  OMX_BUFFERHEADERTYPE *buf_header = NULL;
  OmxAmpAudioRenderer *renderer = static_cast<OmxAmpAudioRenderer *>(context);
  if (CheckMediaHelperEnable(renderer->mMediaHelper,
      PUSH_BUFFER_DONE) == OMX_TRUE) {
    OMX_LOGD("pushBufferDone, port_IO %d, bd 0x%x, bd.bdid 0x%x, bd.allocva 0x%x",
        port_Io, bd, bd->uiBDId, bd->uiAllocVA);
  }
  if (OMX_DirInput == port_Io) {
    if (NULL != renderer->mPool) {
      OMX_U32 ret;
      ret = AMP_CBUFRelease(renderer->mPool, bd);
      if (AMPCBUF_SUCCESS == ret) {
        renderer->mReturnedBdNum++;
        if (CheckMediaHelperEnable(renderer->mMediaHelper,
            PRINT_BD_IN_BACK) == OMX_TRUE) {
          OMX_LOGD("Cbuf [%u/%u] Return Bd from Amp", renderer->mReturnedBdNum,
              renderer->mPushedBdNum);
        }
      } else {
        OMX_LOGE("Cbuf Release Bd error %u.", ret);
      }
      return SUCCESS;
    }
    buf_header = (static_cast<OmxAmpAudioPort *>(renderer->mInputPort))
        ->getBufferHeader(bd);
    if (NULL != buf_header) {
      renderer->mInBDBackNum++;
      renderer->returnBuffer(renderer->mInputPort, buf_header);
    }
    if (CheckMediaHelperEnable(renderer->mMediaHelper,
        PRINT_BD_IN_BACK) == OMX_TRUE) {
      OMX_LOGD("[In] done");
    }
  }

  return SUCCESS;
}

HRESULT OmxAmpAudioRenderer::eventHandle(HANDLE hListener, AMP_EVENT *pEvent,
    VOID *pUserData) {
  if (!pEvent) {
    OMX_LOGE("pEvent is NULL!");
    return !SUCCESS;
  }

  if (AMP_EVENT_API_AREN_CALLBACK == pEvent->stEventHead.eEventCode) {
      OmxAmpAudioRenderer *pComp = static_cast<OmxAmpAudioRenderer *>(pUserData);
      if (pComp) {
        OMX_LOGD("Receive EOS from audio render");
        pComp->postEvent(OMX_EventBufferFlag, kAudioPortStartNumber,
             OMX_BUFFERFLAG_EOS);
      }
  }

  return SUCCESS;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::createComponent(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OMX_LOGD("ENTER");
  OmxAmpAudioRenderer* comp = new OmxAmpAudioRenderer(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  OMX_LOGD("EXIT");
  return OMX_ErrorNone;
}

void OmxAmpAudioRenderer::initAQ() {
  OMX_LOG_FUNCTION_ENTER;
  String8 FILE_SOURCE_URI;
  char ktmp[80] ;
  status_t ret;
  static const char* const kAudioSourcePrefix = "audio://localhost?channel=";
  mAudioChannel = getAudioChannel();
  // TODO:some audio streams can't play by mooplayer but also use OMX.
  // In this case, it can't get RM and mAudioChannel = 0. AVSeting set the
  // audio channel is -0xff. it can use source_constants.h after chao's
  // code was merged: const int AUDIO_SOURCE_NONE_ID = -0xff;
  if ( 0 == mAudioChannel)
    mAudioChannel = - 0xff;

  mSourceControl = new SourceControl();
  if (mSourceControl !=NULL) {
    sprintf(ktmp, "%s%d", kAudioSourcePrefix, mAudioChannel);
    FILE_SOURCE_URI = String8(ktmp);

    mSourceControl->getSourceByUri(FILE_SOURCE_URI, &mSourceId);
    OMX_LOGD("get sourceid :%d, sourceURL :%s", mSourceId, FILE_SOURCE_URI.string());
    if ( -1 != mSourceId) {
      mSourceControl->getSourceGain(mSourceId, &mPhySourceGain);
      OMX_LOGD("get sourGain = %0x", mPhySourceGain);
      mOutputControl = new OutputControl();
      // After being registered , when AVSeting  changes SoureGain value ,
      // OMX can get it by OnSettingUpdate() at once.
      if (mOutputControl != NULL) {
        mObserver = new AVSettingObserver(*this);
        ret = mOutputControl ->registerObserver("mrvl.output", mObserver);
        if (android::OK != ret) {
          OMX_LOGE("rigister Observer failed !!!");
        }
      }
    } else {
      OMX_LOGD("get source ID failed.");
    }
  }
  OMX_LOG_FUNCTION_EXIT;
}

OMX_S32 OmxAmpAudioRenderer::getAudioChannel() {
#if (defined(OMX_IndexExt_h) && defined(_OMX_GTV_))
    if (mResourceInfo.nResourceSize > 0) {
      OMX_LOGD("Resouce size %d, value 0x%x",
          mResourceInfo.nResourceSize, mResourceInfo.nResource[0]);
      return mResourceInfo.nResource[0];
    }
#endif
    return 0;
}

OMX_ERRORTYPE OmxAmpAudioRenderer::setMixGain() {
  HRESULT err = SUCCESS;
  AMP_APP_PARAMIXGAIN stMixerGain;
  if ((mPhySourceGain > 0) && (OMX_TRUE == mIsSnsConnect)) {
    stMixerGain.uiLeftGain = mPhySourceGain;
    stMixerGain.uiRghtGain = mPhySourceGain;
    stMixerGain.uiCntrGain = mPhySourceGain;
    stMixerGain.uiLeftSndGain = mPhySourceGain;
    stMixerGain.uiRghtSndGain = mPhySourceGain;
    stMixerGain.uiLfeGain = mPhySourceGain;
    stMixerGain.uiLeftRearGain = mPhySourceGain;
    stMixerGain.uiRhgtRearGain = mPhySourceGain;
    OMX_LOGD("Set MixGain : %ld", mPhySourceGain);
    err = AMP_SND_SetMixerGain(mSndSrvTunnel[0], &stMixerGain);
    CHECK_AMP_RETURN_VAL(err,"set source service MixerGain");
  }
  return OMX_ErrorNone;
}
void OmxAmpAudioRenderer::AVSettingObserver::OnSettingUpdate(
    const char* name, const AVSettingValue& value) {
    if (strstr(name, "phySourceGain")) {
      mAudioRenderer.mPhySourceGain = value.getInt();
      mAudioRenderer.setMixGain();
    }
}

}
