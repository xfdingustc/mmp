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

#include "MHALAudioDecoder.h"
#include "OMX_AudioExt.h"
#include "OMX_IndexExt.h"
#include "MediaPlayerOnlineDebug.h"

#undef  LOG_TAG
#define LOG_TAG "MHALAudioDecoder"

namespace mmp {
struct AudioCodecEntry {
  const char *mime_type_;
  const char *decoderRole;
  OMX_AUDIO_CODINGTYPE codingType;
};

static const AudioCodecEntry kAudioCodecTable[] = {
  // { codec type, component role, coding type }
  {MMP_MIMETYPE_AUDIO_AAC,           "audio_decoder.aac",    OMX_AUDIO_CodingAAC},
  {MMP_MIMETYPE_AUDIO_MPEG,          "audio_decoder.mp3",    OMX_AUDIO_CodingMP3},
  {MMP_MIMETYPE_AUDIO_MPEG_LAYER_II, "audio_decoder.mp2",
                 static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP2)},
  {MMP_MIMETYPE_AUDIO_MPEG_LAYER_I,  "audio_decoder.mp1",
                 static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP1)},
  {MMP_MIMETYPE_AUDIO_AC3,           "audio_decoder.ac3",
                 static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingAC3)},
  {MMP_MIMETYPE_AUDIO_EAC3,          "audio_decoder.ac3",
                 static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingAC3)},
  {MMP_MIMETYPE_AUDIO_DTS,           "audio_decoder.dts",
                 static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingDTS)},
  {MMP_MIMETYPE_AUDIO_VORBIS,        "audio_decoder.vorbis", OMX_AUDIO_CodingVORBIS},
  {MMP_MIMETYPE_AUDIO_RAW,           "audio_decoder.raw",    OMX_AUDIO_CodingPCM},
  {MMP_MIMETYPE_AUDIO_G711_MLAW,     "audio_decoder.g711",   OMX_AUDIO_CodingG711},
  {MMP_MIMETYPE_AUDIO_G711_ALAW,     "audio_decoder.g711",   OMX_AUDIO_CodingG711},
  {MMP_MIMETYPE_AUDIO_WMA,           "audio_decoder.wma",    OMX_AUDIO_CodingWMA},
  {MMP_MIMETYPE_AUDIO_AMR_NB,        "audio_decoder.amrnb",  OMX_AUDIO_CodingAMR},
  {MMP_MIMETYPE_AUDIO_AMR_WB,        "audio_decoder.amrwb",  OMX_AUDIO_CodingAMR},
  {MMP_MIMETYPE_AUDIO_RA,            "audio_decoder.ra",     OMX_AUDIO_CodingRA},
};

static uint16_t U16LE_AT(const uint8_t *ptr) {
    return ptr[0] | (ptr[1] << 8);
}

static uint32_t U32LE_AT(const uint8_t *ptr) {
    return ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];
}

MHALAudioDecoder::MHALAudioDecoder(MHALOmxCallbackTarget *cb_target)
  : MHALDecoderComponent(OMX_PortDomainAudio) {
  OMX_LOGD("ENTER");
  mCallbackTarget_ = cb_target;
  memset(compName, 0x00, 64);
  sprintf(compName, "\"MHALAudioDecoder\"");
  mNumPorts_ = 2;
  OMX_LOGD("EXIT");
}

MHALAudioDecoder::~MHALAudioDecoder() {
  OMX_LOGD("ENTER");
  OMX_LOGD("EXIT");
}

const char* MHALAudioDecoder::getComponentRole() {
  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return NULL;
  }

  const char* mime_type = NULL;
  caps->GetString("mime_type", &mime_type);

  for (int i = 0; i < NELEM(kAudioCodecTable); ++i) {
    if (!strcmp(mime_type, kAudioCodecTable[i].mime_type_)) {
      OMX_LOGD("find role %s", kAudioCodecTable[i].decoderRole);
      return kAudioCodecTable[i].decoderRole;
    }
  }
  OMX_LOGD("no role found");
  return NULL;
}

OMX_BOOL MHALAudioDecoder::configPort() {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return OMX_FALSE;
  }

  if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    mint32 sample_rate = 0, channels = 0;
    caps->GetInt32("sample_rate", &sample_rate);
    caps->GetInt32("channels", &channels);

    MmpPad* audio_src_pad = new MmpPad("audio_src",
        MmpPad::MMP_PAD_SRC, MmpPad::MMP_PAD_MODE_PUSH);
    MmpCaps &audio_pad_caps = audio_src_pad->GetCaps();
    audio_pad_caps.SetInt32("sample_rate", sample_rate);
    audio_pad_caps.SetInt32("channels", channels);
    audio_src_pad->SetOwner(this);
    AddPad(audio_src_pad);
  }

  const char* mime_type = NULL;
  caps->GetString("mime_type", &mime_type);

  OMX_PORT_PARAM_TYPE oParam;
  InitOmxHeader(&oParam);
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioInit, &oParam);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioInit) with error %d", ret);
    return OMX_FALSE;
  }

  OMX_LOGD("oParam.nStartPortNumber = %d, oParam.nPorts = %d",
      oParam.nStartPortNumber, oParam.nPorts);
  if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_AAC)) {
    setAACFormat(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_MPEG)) {
    setMP3Format(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_MPEG_LAYER_II)) {
    setMP2Format(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_MPEG_LAYER_I)) {
    setMP1Format(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_AC3) ||
      !strcmp(mime_type, MMP_MIMETYPE_AUDIO_EAC3)) {
    setAC3Format(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_VORBIS)) {
    setVorbisFormat(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_G711_MLAW) ||
      !strcmp(mime_type, MMP_MIMETYPE_AUDIO_G711_ALAW)) {
    setG711Format(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_RAW)) {
    setPCMFormat(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_WMA)) {
    setWMAFormat(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_AMR_NB) ||
      !strcmp(mime_type, MMP_MIMETYPE_AUDIO_AMR_WB)) {
    setAMRFormat(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_RA)) {
    setRAFormat(oParam.nStartPortNumber);
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_DTS)) {
    setDTSFormat(oParam.nStartPortNumber);
  }

  OMX_LOGD("EXIT");
  return OMX_TRUE;
}


void MHALAudioDecoder::setAACFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_AACPROFILETYPE profile;
  InitOmxHeader(&profile);
  profile.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioAac, &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioAac) with error %d", ret);
    return;
  }

  profile.nChannels = channels;
  profile.nSampleRate = sample_rate;

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioAac, &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioAac) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}


void MHALAudioDecoder::setMP3Format(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_MP3TYPE profile;
  InitOmxHeader(&profile);
  profile.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioMp3, &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioMp3) with error %d", ret);
    return;
  }

  profile.nChannels = channels;
  profile.nSampleRate = sample_rate;

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioMp3, &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioMp3) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setMP2Format(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_MP3TYPE profile;
  InitOmxHeader(&profile);
  profile.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(
      mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioMp2), &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioMp2) with error %d", ret);
    return;
  }

  profile.nChannels = channels;
  profile.nSampleRate = sample_rate;

  ret = OMX_SetParameter(
      mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioMp2), &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioMp2) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setMP1Format(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_MP3TYPE profile;
  InitOmxHeader(&profile);
  profile.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(
      mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioMp1), &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioMp1) with error %d", ret);
    return;
  }

  profile.nChannels = channels;
  profile.nSampleRate = sample_rate;

  ret = OMX_SetParameter(
      mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioMp1), &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioMp1) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setAC3Format(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_AC3TYPE param;
  InitOmxHeader(&param);
  param.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(
      mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioAc3), &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioAc3) with error %d", ret);
    return;
  }

  param.nChannels = channels;
  param.nSampleRate = sample_rate;

  ret = OMX_SetParameter(
      mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioAc3), &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioAc3) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}


void MHALAudioDecoder::setVorbisFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_VORBISTYPE param;
  InitOmxHeader(&param);
  param.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioVorbis, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioVorbis) with error %d", ret);
    return;
  }

  param.nChannels = channels;
  param.nSampleRate = sample_rate;

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioVorbis, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioVorbis) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}


void MHALAudioDecoder::setG711Format(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;
  const char* mime_type = NULL;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetString("mime_type", &mime_type);
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_PCMMODETYPE param;
  InitOmxHeader(&param);
  param.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioPcm, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioPcm) with error %d", ret);
    return;
  }

  param.eNumData = OMX_NumericalDataSigned;
  param.nBitPerSample = 8;
  param.eEndian = OMX_EndianLittle;
  param.bInterleaved = OMX_TRUE;
  param.nSamplingRate = sample_rate;
  param.nChannels = channels;
  if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_G711_ALAW)) {
    param.ePCMMode = OMX_AUDIO_PCMModeALaw;
  } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_G711_MLAW)) {
    param.ePCMMode = OMX_AUDIO_PCMModeMULaw;
  }

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioPcm, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioPcm) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setPCMFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  mint32 sample_rate = 0, channels = 0;
  mint32 bits_per_sample = 0;
  mint32 data_sign = 0; // 0: signed, 1: unsigned
  mint32 data_endian = 0; // 0: big endian, 1: little endian
  mint32 pcm_mode = 0; // 0: PCMModeLinear, 1: PCMModeBluray

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);
  caps->GetInt32("bits_per_sample", &bits_per_sample);
  caps->GetInt32("data_sign", &data_sign);
  caps->GetInt32("data_endian", &data_endian);
  caps->GetInt32("pcm_mode", &pcm_mode);

  OMX_AUDIO_PARAM_PCMMODETYPE param;
  InitOmxHeader(&param);
  param.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioPcm, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioPcm) with error %d", ret);
    return;
  }

  param.nSamplingRate = sample_rate;
  param.nChannels = channels;
  param.nBitPerSample = bits_per_sample;
  param.eNumData = static_cast<OMX_NUMERICALDATATYPE>(data_sign);
  param.eEndian = static_cast<OMX_ENDIANTYPE>(data_endian);
  param.bInterleaved = OMX_TRUE;
  param.ePCMMode = OMX_AUDIO_PCMModeLinear;
  if (1 == pcm_mode) {
    param.ePCMMode = static_cast<OMX_AUDIO_PCMMODETYPE>(OMX_AUDIO_PCMModeBluray);
  }
  OMX_LOGE("param.nBitPerSample = %d, SamplingRate = %d, Channels = %d",
      param.nBitPerSample, param.nSamplingRate, param.nChannels);

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioPcm, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioPcm) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setWMAFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint32_t encodeOptions = 0;
  uint64_t superBlockAlign = 0;
  uint32_t advancedEncodeOpt = 0;
  uint32_t advancedEncodeOpt2 = 0;
  const char *mime_type = NULL;
  const void *extradata = NULL;
  mint32 sample_rate = 0, channels = 0;
  mint32 extradata_size = 0;
  mint32 bits_per_sample = 0, codec_tag = 0, bit_rate = 0, block_align = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetString("mime_type", &mime_type);
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);
  caps->GetData("codec-specific-data", &extradata, &extradata_size);
  caps->GetInt32("bits_per_sample", &bits_per_sample);
  caps->GetInt32("codec_tag", &codec_tag);
  caps->GetInt32("bit_rate", &bit_rate);
  caps->GetInt32("block_align", &block_align);

  uint8_t *data = (uint8_t *)extradata;
  if (extradata_size >= 10) {
    encodeOptions = U16LE_AT(data + 4);
    superBlockAlign = U32LE_AT(data + 6);
  }
  if (extradata_size >= 18) {
    advancedEncodeOpt = U32LE_AT(data + 10);
    advancedEncodeOpt2 = U32LE_AT(data + 14);
  }

  OMX_AUDIO_PARAM_WMATYPE param;
  InitOmxHeader(&param);
  param.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioWma, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioWma) with error %d", ret);
    return;
  }

  param.nChannels = channels;
  param.nBitRate = bit_rate;
  param.nBitsPerSample = bits_per_sample;
  param.eProfile = OMX_AUDIO_WMAProfileUnused;
  param.nSamplingRate = sample_rate;
  param.nBlockAlign = block_align;
  param.nEncodeOptions = encodeOptions;
  param.nSuperBlockAlign = superBlockAlign;
  param.nAdvancedEncodeOpt = advancedEncodeOpt;
  param.nAdvancedEncodeOpt2 = advancedEncodeOpt2;
  switch (codec_tag) {
    case 0x0161:
        param.eFormat = OMX_AUDIO_WMAFormat9;
        break;
    case 0x0162:
        param.eFormat = OMX_AUDIO_WMAFormat9_Professional;
        break;
    case 0x0163:
        param.eFormat = OMX_AUDIO_WMAFormat9_Lossless;
        break;
    default:
        OMX_LOGE("Unsupported wma codec tag 0x%x", codec_tag);
        break;
  }

  OMX_LOGE("BitRate = %d, SamplingRate = %d, Channels = %d, BitsPerSample = %d",
      param.nBitRate, param.nSamplingRate, param.nChannels, param.nBitsPerSample);
  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioWma, &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioWma) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setAMRFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_MP3TYPE profile;
  InitOmxHeader(&profile);
  profile.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioAmr, &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioMp3) with error %d", ret);
    return;
  }

  profile.nChannels = channels;
  profile.nSampleRate = sample_rate;

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioAmr, &profile);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioMp3) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setRAFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  const void *extradata = NULL;
  mint32 sample_rate = 0, channels = 0;
  mint32 extradata_size = 0;
  mint32 bits_per_sample = 0, sub_packet_size = 0, flavor_index = 0, coded_framesize = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);
  caps->GetData("codec-specific-data", &extradata, &extradata_size);
  caps->GetInt32("bits_per_sample", &bits_per_sample);
  caps->GetInt32("sub_packet_size", &sub_packet_size);
  caps->GetInt32("flavor_index", &flavor_index);
  caps->GetInt32("coded_framesize", &coded_framesize);

  OMX_AUDIO_PARAM_EXT_RATYPE raType;
  InitOmxHeader(&raType);
  raType.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioRa, &raType);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioRa) with error %d", ret);
    return;
  }

  raType.eFormat = OMX_AUDIO_RA8; // Only support cook at the moment.
  raType.nChannels = channels;
  raType.nSamplingRate = sample_rate;
  raType.nBitsPerFrame = bits_per_sample;
  if (raType.nBitsPerFrame <= 0) {
    raType.nBitsPerFrame = 16;
  }
  raType.nSamplePerFrame = sub_packet_size;
  raType.nFlavorIndex = flavor_index;
  raType.nGranularity = coded_framesize;
  raType.nOpaqueDataSize = extradata_size;
  if(extradata_size <= 32) {
    memcpy(raType.nOpaqueData, extradata, extradata_size);
  }

  OMX_LOGD("Channels %d, SamplingRate %d, BitsPerFrame %d, SamplePerFrame %d",
      raType.nChannels, raType.nSamplingRate, raType.nBitsPerFrame, raType.nSamplePerFrame);
  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamAudioRa, &raType);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioRa) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::setDTSFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 sample_rate = 0, channels = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("sample_rate", &sample_rate);
  caps->GetInt32("channels", &channels);

  OMX_AUDIO_PARAM_DTSTYPE param;
  InitOmxHeader(&param);
  param.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioDts), &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioDts) with error %d", ret);
    return;
  }

  param.nChannels = channels;
  param.nSampleRate = sample_rate;

  ret = OMX_SetParameter(mCompHandle_, static_cast<OMX_INDEXTYPE>(OMX_IndexParamAudioDts), &param);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioDts) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALAudioDecoder::pushCodecConfigData() {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  const char *mime_type = NULL;
  const void *extradata = NULL;
  mint32 extradata_size = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetString("mime_type", &mime_type);
  caps->GetData("codec-specific-data", &extradata, &extradata_size);

  if (extradata_size > 0) {
    OMX_LOGD("push audio codec config data.");
    kdThreadMutexLock(mInBufferLock_);
    OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers_.begin());
    if (buf && buf->pBuffer) {
      kdMemcpy(buf->pBuffer, extradata, extradata_size);
      buf->nFilledLen = extradata_size;
      buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
      ret = OMX_EmptyThisBuffer(mCompHandle_, buf);
      if (OMX_ErrorNone != ret) {
        OMX_LOGE("push audio codec config data failed.");
      }
      mInputBuffers_.erase(mInputBuffers_.begin());
    }
    kdThreadMutexUnlock(mInBufferLock_);
  }
  OMX_LOGD("EXIT");
}

OMX_BOOL MHALAudioDecoder::queueEOSToOMX() {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  kdThreadMutexLock(mInBufferLock_);

  if (mInputBuffers_.empty()) {
    kdThreadMutexUnlock(mInBufferLock_);
    return OMX_FALSE;
  }

  OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers_.begin());
  if (!buf || !buf->pBuffer) {
    OMX_LOGE("OMX_BUFFERHEADERTYPE is NULL.");
    kdThreadMutexUnlock(mInBufferLock_);
    return OMX_FALSE;
  }

  buf->nFlags = OMX_BUFFERFLAG_EOS;
  buf->nFilledLen = 0;
  mInPortEOS_ = OMX_TRUE;
  OMX_LOGI("Input port got EOS!");
  ret = OMX_EmptyThisBuffer(mCompHandle_, buf);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("OMX_EmptyThisBuffer() failed with error %d", ret);
  }

  mInputBuffers_.erase(mInputBuffers_.begin());

  kdThreadMutexUnlock(mInBufferLock_);
  OMX_LOGD("EXIT");
  return OMX_TRUE;
}

OMX_BOOL MHALAudioDecoder::queueBufferToOMX(MmpBuffer* pkt) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  if (!pkt) {
    OMX_LOGE("pkt is NULL!");
    return OMX_FALSE;
  }

  kdThreadMutexLock(mInBufferLock_);

  if (mInputBuffers_.empty()) {
    kdThreadMutexUnlock(mInBufferLock_);
    return OMX_FALSE;
  }

  OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers_.begin());
  if (!buf || !buf->pBuffer) {
    OMX_LOGE("OMX_BUFFERHEADERTYPE is NULL.");
    kdThreadMutexUnlock(mInBufferLock_);
    return OMX_FALSE;
  }

  if (0 == buf->nAllocLen) {
    OMX_LOGE("buf->nAllocLen is 0, bad OMX_BUFFERHEADERTYPE!");
    kdThreadMutexUnlock(mInBufferLock_);
    return OMX_FALSE;
  }

  int buffers_need = pkt->size / buf->nAllocLen;
  if (pkt->size % buf->nAllocLen) {
    buffers_need += 1;
  }
  if (mInputBuffers_.size() < buffers_need) {
    OMX_LOGD("no enough buffer, pkt size %d bytes, buffer size %d bytes: "
        "requires %d buffers but only %d left, sleep 10ms and try again",
        pkt->size, buf->nAllocLen, buffers_need, mInputBuffers_.size());
    kdThreadMutexUnlock(mInBufferLock_);
    return OMX_FALSE;
  }

  int bytes_sent = 0;
  while (bytes_sent < pkt->size) {
    buf = *(mInputBuffers_.begin());
    if (!buf || !buf->pBuffer) {
      OMX_LOGE("OMX_BUFFERHEADERTYPE is NULL.");
      break;
    }

    //TODO: remove this patch when stream in mode is implemented
    buf->nTimeStamp = kInvalidTimeStamp;
    buf->nFlags = 0;
    if(bytes_sent == 0) {
        //the first buffer for this frame
        buf->nTimeStamp = pkt->pts;
        if (pkt->IsFlagSet(MmpBuffer::MMP_BUFFER_FLAG_KEY_FRAME)) {
            buf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
        }
    }

    if (pkt->size - bytes_sent > buf->nAllocLen) {
      // TODO: should send less than buf->nAllocLen,
      // because OMX IL may merge codec config data
      kdMemcpy(buf->pBuffer, pkt->data + bytes_sent, buf->nAllocLen);
      buf->nFilledLen = buf->nAllocLen;
      bytes_sent += buf->nFilledLen;
    } else {
      kdMemcpy(buf->pBuffer, pkt->data + bytes_sent, pkt->size - bytes_sent);
      buf->nFilledLen = pkt->size - bytes_sent;
      bytes_sent += buf->nFilledLen;
      if (pkt->size > buf->nAllocLen) {
        buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
      }
    }

    OMX_LOGD("Push frame, pts = "TIME_FORMAT"(%lld us), size = %lu, flags = 0x%x",
        TIME_ARGS(buf->nTimeStamp), buf->nTimeStamp, buf->nFilledLen, buf->nFlags);
    ret = OMX_EmptyThisBuffer(mCompHandle_, buf);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("OMX_EmptyThisBuffer() failed with error %d", ret);
    }
    mInputBuffers_.erase(mInputBuffers_.begin());
  }

  kdThreadMutexUnlock(mInBufferLock_);
  return OMX_TRUE;
}

// If there is data to process and successfully handled, return OMX_TRUE.
// Else, return OMX_FALSE.
OMX_BOOL MHALAudioDecoder::processData() {
  MmpBuffer* pkt = NULL;
  OMX_BOOL result = OMX_TRUE;

  if ((OMX_TRUE != mCanFeedData_) || mInPortEOS_) {
    return OMX_FALSE;
  }

  kdThreadMutexLock(mPacketListLock_);
  pkt = mDataList_->begin();
  if (!pkt) {
    kdThreadMutexUnlock(mPacketListLock_);
    return OMX_FALSE;
  }

  if (pkt->IsFlagSet(MmpBuffer::MMP_BUFFER_FLAG_EOS)) {
    result = queueEOSToOMX();
  } else {
    result = queueBufferToOMX(pkt);
  }

  if (result) {
    // Packet is successfully sent to OMX, so remove it from list.
    pkt = mDataList_->popFront();
    delete pkt;
    pkt = NULL;
  }

  kdThreadMutexUnlock(mPacketListLock_);
  return result;
}

OMX_ERRORTYPE MHALAudioDecoder::OnOMXEvent(OMX_EVENTTYPE eEvent,
    OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData) {
  switch (eEvent) {
    case OMX_EventBufferFlag:
      OMX_LOGI("event = OMX_EventBufferFlag, data1 = %lu, data2 = %lu", nData1, nData2);
      if ((nData2 & OMX_BUFFERFLAG_EOS) != 0) {
        OMX_LOGI("EOS received!!!");
        mOutPortEOS_ = OMX_TRUE;
        if ((OMX_NON_TUNNEL == omx_tunnel_mode_) && mCallbackTarget_) {
          mCallbackTarget_->OnOmxEvent(EVENT_AUDIO_EOS);
        }
      }
      break;

    default:
      return MHALOmxComponent::OnOMXEvent(eEvent, nData1, nData2, pEventData);
  }

  return OMX_ErrorNone;
}

void MHALAudioDecoder::fillThisBuffer(MmpBuffer *buf) {
  // Secondly, send this buffer to OMX again to fill this buffer.
  if (!mOutPortEOS_) {
    for (OMX_U32 j = 0; j < mOutputBuffers_.size(); j++) {
      BufferInfo *info = mOutputBuffers_[j];
      if (info && info->buffer == buf) {
        OMX_FillThisBuffer(mCompHandle_, info->header);
      }
    }
  }
}

OMX_ERRORTYPE MHALAudioDecoder::FillBufferDone(OMX_BUFFERHEADERTYPE* pBuffer) {
  // Firstly, push this buffer to audio render.
  for (OMX_U32 j = 0; j < mOutputBuffers_.size(); j++) {
    BufferInfo *info = mOutputBuffers_[j];
    if (info && info->header == pBuffer) {
      MmpPad* src_pad = GetSrcPadByPrefix("audio_src", 0);
      if (src_pad) {
        info->buffer->size = pBuffer->nFilledLen;
        src_pad->Push(info->buffer);
      }
    }
  }

  return OMX_ErrorNone;
}


OMX_BOOL MHALAudioDecoder::isAudioSupported(const char *mime_type) {
  if (!mime_type) {
    OMX_LOGE("No mime type found!");
    return OMX_FALSE;
  }
  for (int i = 0; i < NELEM(kAudioCodecTable); ++i) {
    if (!strcmp(mime_type, kAudioCodecTable[i].mime_type_)) {
      return OMX_TRUE;
    }
  }
  return OMX_FALSE;
}
}
