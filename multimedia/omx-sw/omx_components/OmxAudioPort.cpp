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

#define LOG_TAG "OmxAudioPort"
#include <OmxAudioPort.h>

namespace mmp {

OmxAudioPort::OmxAudioPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;

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

  InitOmxHeader(mAudioParam);
  updateDomainParameter();
};

OmxAudioPort::~OmxAudioPort(void) {
}

void OmxAudioPort::SetOutputParameter (OMX_U32 mSampleRate, OMX_U32 mChannel) {
  mCodecParam.pcmout.nChannels = mChannel;
  mCodecParam.pcmout.nSamplingRate = mSampleRate;
}

/************************
*  OmxMP3Port
************************/

OmxMp3Port::OmxMp3Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  mDefinition.nBufferCountMin = kNumBuffers;
  mDefinition.nBufferCountActual = mDefinition.nBufferCountMin;
  mDefinition.bBuffersContiguous = OMX_FALSE;

  if (dir == OMX_DirInput) {
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp3);
  mCodecParam.mp3.nPortIndex = mDefinition.nPortIndex;
}

OmxMp3Port::OmxMp3Port(OMX_U32 index, OMX_DIRTYPE dir) {
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
  } else if (dir == OMX_DirOutput) {
    mDefinition.nBufferSize = kOutputBufferSize;
    mDefinition.nBufferAlignment = 2;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  } else {
    OMX_LOGE("Error dir %d\n", dir);
  }
  mDefinition.format.audio.pNativeRender = NULL;
  mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mp3);
  mCodecParam.mp3.nPortIndex = mDefinition.nPortIndex;
}

OmxMp3Port::~OmxMp3Port() {
}


/************************
*  OmxAACPort
************************/

OmxAACPort::OmxAACPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
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
  InitOmxHeader(&mCodecParam.aac);
  mCodecParam.aac.nPortIndex = mDefinition.nPortIndex;
}

OmxAACPort::OmxAACPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
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
  InitOmxHeader(&mCodecParam.aac);
  mCodecParam.aac.nPortIndex = mDefinition.nPortIndex;
}

OmxAACPort::~OmxAACPort() {
}

/************************
*  OmxVorbisPort
************************/

OmxVorbisPort::OmxVorbisPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
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
  InitOmxHeader(&mCodecParam.vorbis);
  mCodecParam.aac.nPortIndex = mDefinition.nPortIndex;
}

OmxVorbisPort::OmxVorbisPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainAudio;
  if (dir == OMX_DirInput) {
    mDefinition.nBufferCountMin = kNumInputBuffers;
    mDefinition.nBufferSize = 8192;
    mDefinition.nBufferAlignment = 1;
    mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
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
  InitOmxHeader(&mCodecParam.vorbis);
  mCodecParam.aac.nPortIndex = mDefinition.nPortIndex;
}


OmxPcmPort::OmxPcmPort(OMX_DIRTYPE dir) {
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
}

OmxPcmPort::OmxPcmPort(OMX_U32 index, OMX_DIRTYPE dir) {
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
}

}
