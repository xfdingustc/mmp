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
 * @file BerlinOmxAmpAudioPort.cpp
 * @author csheng@marvell.com
 * @date 2013.01.17
 * @brief Manager OmxAmpAudioPort and OmxAmpAacPort
 * @todo need add other ports such as mp3port ddport...
 */

#define LOG_TAG "OmxAmpAudioPort"
#include <BerlinOmxAmpAudioPort.h>

namespace berlin {

OmxAmpAudioPort::OmxAmpAudioPort() {
  InitOmxHeader(&mAudioParam);
}

OMX_U8 OmxAmpAudioPort::formatAudEsData(OMX_BUFFERHEADERTYPE *header) {
  return mFormatHeadSize;
}

/**
 * @param index port index
 * @param dir port type (input or output)
 * @todo need init some thing
 */
OmxAmpAudioPort::OmxAmpAudioPort(OMX_U32 index, OMX_DIRTYPE dir) :
    mFormatHeadSize(0) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;

  mDefinition.bEnabled = OMX_TRUE;
  mDefinition.bPopulated = OMX_FALSE;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
    InitOmxHeader(&mCodecParam.pcmout);
    mCodecParam.pcmout.eNumData = OMX_NumericalDataSigned;
    mCodecParam.pcmout.eEndian = OMX_EndianLittle;
    mCodecParam.pcmout.bInterleaved = OMX_TRUE;
    mCodecParam.pcmout.nBitPerSample = 16;
    mCodecParam.pcmout.ePCMMode = OMX_AUDIO_PCMModeLinear;
    mCodecParam.pcmout.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    mCodecParam.pcmout.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
    mCodecParam.pcmout.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
    mCodecParam.pcmout.eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
    mCodecParam.pcmout.eChannelMapping[4] = OMX_AUDIO_ChannelLS;
    mCodecParam.pcmout.eChannelMapping[5] = OMX_AUDIO_ChannelRS;
    mCodecParam.pcmout.nChannels = kDefaultOutputChannels;
    mCodecParam.pcmout.nSamplingRate = kDefaultOutputSampleRate;
    mCodecParam.pcmout.nPortIndex = mDefinition.nPortIndex;
  }

  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  InitOmxHeader(&mAudioParam);
  updateDomainParameter();
  mPushedBytes = 0;
  mPushedOffset = 0;
}

/**
 * @todo need deinit some thing
 */
OmxAmpAudioPort::~OmxAmpAudioPort(void) {
}

void OmxAmpAudioPort::SetOutputParameter
    (OMX_U32 mSampleRate, OMX_U32 mChannel) {
  if (mChannel == 1 && isTunneled() == OMX_FALSE) {
    mChannel = 2;
  }
  mCodecParam.pcmout.nChannels = mChannel;
  mCodecParam.pcmout.nSamplingRate = mSampleRate;
}

void OmxAmpAudioPort::GetOutputParameter
    (OMX_U32 *mSampleRate, OMX_U32 *mChannel) {
  if (mSampleRate != NULL) {
    *mSampleRate = mCodecParam.pcmout.nSamplingRate;
  }
  if (mChannel != NULL) {
    *mChannel = mCodecParam.pcmout.nChannels;
  }
}

OMX_ERRORTYPE OmxAmpAudioPort::configMemInfo(AMP_SHM_HANDLE es_handle) {
  HRESULT err = SUCCESS;
  mEs = es_handle;
  err = AMP_SHM_GetVirtualAddress(mEs, 0, (VOID **)&mEsVirtAddr);
  CHECKAMPERR(err);
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpAudioPort::updateMemInfo(OMX_BUFFERHEADERTYPE *header) {
  OMX_LOGV("DMX size %d pushedoffset %d buffer size %d\n",
      DMX_ES_BUF_SIZE, mPushedOffset, header->nFilledLen);
  if (DMX_ES_BUF_SIZE > mPushedOffset + header->nFilledLen) {
    kdMemcpy(mEsVirtAddr + mPushedOffset, header->pBuffer + header->nOffset,
        header->nFilledLen);
    mPushedOffset += header->nFilledLen;
  } else {
    kdMemcpy(mEsVirtAddr + mPushedOffset, header->pBuffer + header->nOffset,
        DMX_ES_BUF_SIZE - mPushedOffset);
    kdMemcpy(mEsVirtAddr,
        header->pBuffer + header->nOffset + (DMX_ES_BUF_SIZE - mPushedOffset),
        mPushedOffset + header->nFilledLen - DMX_ES_BUF_SIZE);
    mPushedOffset = mPushedOffset + header->nFilledLen - DMX_ES_BUF_SIZE;
  }

  mPushedBytes += header->nFilledLen;
  return OMX_ErrorNone;
}

/**
 * @brief get format header size
 */
OMX_U32 OmxAmpAudioPort::getFormatHeadSize() {
  return mFormatHeadSize;
}

/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpAacPort
 */
OmxAmpAacPort::OmxAmpAacPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/aac");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.aac);
  mCodecParam.aac.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
  mPushedOffset = 0;
  mPushedBytes = 0;
  mIsADTS = OMX_TRUE;
  mAACHeader = NULL;
}

/**
 * @brief Init OmxAmpAacPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpAacPort::OmxAmpAacPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/aac");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.aac);
  mCodecParam.aac.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
  mPushedOffset = 0;
  mPushedBytes = 0;
  mIsADTS = OMX_TRUE;
  mAACHeader = NULL;
}

/**
 * @brief set aac codec parameter
 * @param codec_param aac codec parameters
 */
inline void OmxAmpAacPort::setCodecParam(
    OMX_AUDIO_PARAM_AACPROFILETYPE *codec_param, OMX_U8 *esds) {
  OMX_LOGD("setCodecParam codec_param %p mIsADTS %d esds %p\n",
      codec_param, mIsADTS, esds);
  if (codec_param != NULL) {
    mCodecParam.aac = *codec_param;
    OMX_LOGD("eAACStreamFormat %d\n", codec_param->eAACStreamFormat);
    if (codec_param->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4ADTS ||
        codec_param->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4LATM) {
      mIsADTS = OMX_TRUE;
      // adts dont need to add header
      return;
    } else {
      mIsADTS = OMX_FALSE;
    }
  }
  if (mIsADTS == OMX_TRUE || esds == NULL) {
    return;
  }
  if (NULL != mAACHeader) {
    kdFree(mAACHeader);
    mAACHeader = NULL;
  }
  mFormatHeadSize = kADTSHeaderSize;
  mAACHeader = static_cast<OMX_U8 *>(kdMalloc(mFormatHeadSize));
  if (mAACHeader == NULL) {
    OMX_LOGE("Malloc mAACHeader size %d fail", mFormatHeadSize);
    return;
  }
  kdMemset(mAACHeader, 0x0, mFormatHeadSize);
  // add adts header from vendor/tv/frameworks/av/libs/mooplayer/ADTSHeaderWriter.cpp
  OMX_LOGD("esds[0] is %d esds[1] is %d\n", esds[0], esds[1]);
  OMX_U32 profileIndex = (esds[0] >> 3) - 1;
  OMX_U32 samplingFreqIndex = ((esds[0] << 1) | (esds[1] >> 7)) & 0xF;
  OMX_U32 channelCfgIndex = (esds[1] >> 3);
  OMX_LOGD("profileIndex %d samplingFreqIndex %d channelCfgIndex %d\n",
      profileIndex, samplingFreqIndex, channelCfgIndex);
  // Syncword 0xFFF;
  mAACHeader[0] = 0xFF;
  // MPEG-4; Layer = 0; Protection absent = 1;
  mAACHeader[1] = 0xF1;
  // Profile index; Sampling frequency index; private stream = 0;
  // Channel configuration index;
  mAACHeader[2] = ((profileIndex << 6) |
                       (samplingFreqIndex << 2) |
                       ((channelCfgIndex >> 2) & 0x1)) & 0xFF;
  // originality = 0; home = 0; copyrighted stream = 0;
  // copyrighted stream start = 0; Frame size = 0 (for template);
  mAACHeader[3] = (channelCfgIndex << 6) & 0xFF;
  mAACHeader[4] = 0;
  // Buffer fullness = 0x7FF (lie);
  mAACHeader[5] = 0x1F;
  // # of AAC frames - 1 = 0;
  mAACHeader[6] = 0xFC;
};

/**
 * @brief format es data, add aac header
 */
OMX_U8 OmxAmpAacPort::formatAudEsData(OMX_BUFFERHEADERTYPE *header) {
  if (mFormatHeadSize == 0 || header->nFilledLen == 0 || mAACHeader == NULL)
    return 0;
  OMX_U8 *src = header->pBuffer + header->nOffset;
  OMX_LOGV("filled old %ld, new %ld header %u",
      header->nFilledLen, header->nFilledLen + mFormatHeadSize, mFormatHeadSize);
  kdMemmove(src + mFormatHeadSize, src, header->nFilledLen);
  header->nFilledLen += mFormatHeadSize;
  kdMemcpy(src, mAACHeader, mFormatHeadSize);
  src[3] |= ((header->nFilledLen >> 11) & 3) & 0xFF;
  src[4] |= (header->nFilledLen >> 3) & 0xFF;
  src[5] |= (header->nFilledLen << 5) & 0xFF;

  return mFormatHeadSize;
}

/**
 * @brief Deinit OmxAmpAacPort
 */
OmxAmpAacPort::~OmxAmpAacPort() {
  if (NULL != mAACHeader) {
    kdFree(mAACHeader);
    mAACHeader = NULL;
  }
}

#ifdef OMX_AudioExt_h
/**
 *@brief Init OmxAmpMp1Port
 *@param dir OMX port type (input or output)
 */
OmxAmpMp1Port::OmxAmpMp1Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP1);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/mpeg");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp2);
  mCodecParam.mp2.nPortIndex = mDefinition.nPortIndex;
}

/**
 *@brief Init OmxAmpMp1Port
 *@param index OMX port index
 *@param dir OMX port type (input or output)
 */
OmxAmpMp1Port::OmxAmpMp1Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP1);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/mpeg");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp2);
  mCodecParam.mp2.nPortIndex = mDefinition.nPortIndex;
}

/**
 *@brief Deinit OmxAmpMp1Port
 */
OmxAmpMp1Port::~OmxAmpMp1Port() {
}



/**
 *@brief Init OmxAmpMp2Port
 *@param dir OMX port type (input or output)
 */
OmxAmpMp2Port::OmxAmpMp2Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP2);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/mpeg");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp2);
  mCodecParam.mp2.nPortIndex = mDefinition.nPortIndex;
}

/**
 *@brief Init OmxAmpMp2Port
 *@param index OMX port index
 *@param dir OMX port type (input or output)
 */
OmxAmpMp2Port::OmxAmpMp2Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP2);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/mpeg");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp2);
  mCodecParam.mp2.nPortIndex = mDefinition.nPortIndex;
}

/**
 *@brief Deinit OmxAmpMp2Port
 */
OmxAmpMp2Port::~OmxAmpMp2Port() {
}

/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpAc3Port
 */
OmxAmpAc3Port::OmxAmpAc3Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingAC3);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/ac3");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.ac3);
  mCodecParam.ac3.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
  mPushedBytes = 0;
  mPushedOffset = 0;
}

/**
 * @brief Init OmxAmpAc3Port
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpAc3Port::OmxAmpAc3Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingAC3);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/ac3");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.ac3);
  mCodecParam.ac3.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
  mPushedBytes = 0;
  mPushedOffset = 0;
}

/**
 * @brief Deinit OmxAmpAc3Port
 */
OmxAmpAc3Port::~OmxAmpAc3Port() {
}

/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpDtsPort
 */
OmxAmpDtsPort::OmxAmpDtsPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 32 * 1024 + 64;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingDTS);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/dts");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.dts);
  mCodecParam.dts.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Init OmxAmpDtsPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpDtsPort::OmxAmpDtsPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 32 * 1024 + 64;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding =
        static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingDTS);
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/dts");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.dts);
  mCodecParam.dts.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Deinit OmxAmpDtsPort
 */
OmxAmpDtsPort::~OmxAmpDtsPort() {
}
#endif

/**
 *@brief Init OmxAmpMp3Port
 *@param dir OMX port type (input or output)
 */
OmxAmpMp3Port::OmxAmpMp3Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/mpeg");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp3);
  mCodecParam.mp3.nPortIndex = mDefinition.nPortIndex;
}

/**
 *@brief Init OmxAmpMp3Port
 *@param index OMX port index
 *@param dir OMX port type (input or output)
 */
OmxAmpMp3Port::OmxAmpMp3Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/mpeg");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp3);
  mCodecParam.mp3.nPortIndex = mDefinition.nPortIndex;
}

/**
 *@brief Deinit OmxAmpMp3Port
 */
OmxAmpMp3Port::~OmxAmpMp3Port() {
}

/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpVorbisPort
 */
OmxAmpVorbisPort::OmxAmpVorbisPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/vorbis");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.vorbis);
  mCodecParam.vorbis.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Init OmxAmpVorbisPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpVorbisPort::OmxAmpVorbisPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/vorbis");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.vorbis);
  mCodecParam.vorbis.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Deinit OmxAmpVorbisPort
 */
OmxAmpVorbisPort::~OmxAmpVorbisPort() {
}

/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpPcmPort
 */
OmxAmpPcmPort::OmxAmpPcmPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192 * 2;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/pcm");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.pcm);
  mCodecParam.pcm.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Init OmxAmpPcmPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpPcmPort::OmxAmpPcmPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192 * 2;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/pcm");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.pcm);
  mCodecParam.pcm.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Deinit OmxAmpPcmPort
 */
OmxAmpPcmPort::~OmxAmpPcmPort() {
}


/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpWmaPort
 */
OmxAmpWmaPort::OmxAmpWmaPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192 * 4 + 16;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/wma");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.wma);
  mCodecParam.wma.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Init OmxAmpWmaPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpWmaPort::OmxAmpWmaPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192 * 4 + 16;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/wma");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.wma);
  mCodecParam.wma.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Deinit OmxAmpWmaPort
 */
OmxAmpWmaPort::~OmxAmpWmaPort() {
}

/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpAmrPort
 */
OmxAmpAmrPort::OmxAmpAmrPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAMR;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/amr");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.amr);
  mCodecParam.amr.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Init OmxAmpAmrPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpAmrPort::OmxAmpAmrPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAMR;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/amr");
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    // mDefinition.format.audio.cMIMEType = const_cast<char *>("audio/raw");
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.amr);
  mCodecParam.amr.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Deinit OmxAmpAmrPort
 */
OmxAmpAmrPort::~OmxAmpAmrPort() {
}

#ifdef ENABLE_EXT_RA
/**
 * @param dir OMX port type (input or output)
 * @brief Init OmxAmpRaPort
 */
OmxAmpRaPort::OmxAmpRaPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingRA;
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.ra);
  mCodecParam.ra.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Init OmxAmpRaPort
 * @param index OMX port index
 * @param dir OMX port type (input or output)
 */
OmxAmpRaPort::OmxAmpRaPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingRA;
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferCountMin = kNumOutputBuffers;
    mDefinition.nBufferSize = 4096 * kMaxOutPutChannels;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.ra);
  mCodecParam.ra.nPortIndex = mDefinition.nPortIndex;
  mFormatHeadSize = 0;
}

/**
 * @brief Deinit OmxAmpRaPort
 */
OmxAmpRaPort::~OmxAmpRaPort() {
}
#endif
}
