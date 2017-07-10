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

#include "MHALVideoDecoder.h"
#include "OMX_VideoExt.h"
#include "MediaPlayerOnlineDebug.h"

#undef  LOG_TAG
#define LOG_TAG "MHALVideoDecoder"

namespace mmp {

typedef struct _Common_packet_header {
  uint16_t start_code_1;
  uint8_t  start_code_2;
  uint8_t  packet_type;
  uint16_t reserved;
  uint8_t  marker_byte_1;
  uint16_t packet_payload_len_msb;
  uint8_t  marker_byte_2;
  uint16_t packet_payload_len_lsb;
  uint8_t  marker_byte_3;
  uint16_t packet_padding_len;
  uint8_t marker_byte_4;
} __attribute__ ((packed))RmPacketHeader;

typedef struct _Stream_payload {
  uint16_t pic_dis_width;
  uint16_t pic_dis_height;
  uint32_t frame_rate;
  uint32_t spo_flags;
  uint32_t bitstream_version;
  uint32_t encode_size;
  uint16_t resampled_image_width;
  uint16_t resampled_image_height;
} RmStreamPayHeader;

struct VideoCodecEntry {
  const char *mime_type_;
  const char *decoderRole;
  OMX_VIDEO_CODINGTYPE codingType;
};

static const VideoCodecEntry kVideoCodecTable[] = {
  // { mime type, component role, coding type }
  {MMP_MIMETYPE_VIDEO_AVC,      "video_decoder.avc",   OMX_VIDEO_CodingAVC},
  {MMP_MIMETYPE_VIDEO_MPEG4,    "video_decoder.mpeg4", OMX_VIDEO_CodingMPEG4},
  {MMP_MIMETYPE_VIDEO_MSMPEG4,  "video_decoder.msmpeg4",
      static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingMSMPEG4)},
  {MMP_MIMETYPE_VIDEO_MPEG2,    "video_decoder.mpeg2", OMX_VIDEO_CodingMPEG2},
  {MMP_MIMETYPE_VIDEO_VC1,      "video_decoder.vc1",   OMX_VIDEO_CodingVC1},
  //{AV_CODEC_ID_MJPEG,      "video_decoder.mjpeg", OMX_VIDEO_CodingMJPEG},
  //{AV_CODEC_ID_VP6,        "video_decoder.vp6",
  //    static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVP6)},
  //{AV_CODEC_ID_VP6F,       "video_decoder.vp6",
  //    static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVP6)},
  {MMP_MIMETYPE_VIDEO_VPX,      "video_decoder.vp8",   OMX_VIDEO_CodingVP8},
  {MMP_MIMETYPE_VIDEO_H263,     "video_decoder.h263",  OMX_VIDEO_CodingH263},
  {MMP_MIMETYPE_VIDEO_WMV,      "video_decoder.wmv",   OMX_VIDEO_CodingWMV},
  {MMP_MIMETYPE_VIDEO_RV,       "video_decoder.rv",    OMX_VIDEO_CodingRV},
};

uint16_t U16_AT(const uint8_t *ptr) {
    return ptr[0] << 8 | ptr[1];
}

MHALVideoDecoder::MHALVideoDecoder(MHALOmxCallbackTarget *cb_target)
  : MHALDecoderComponent(OMX_PortDomainVideo) {
  OMX_LOGD("ENTER");
  mCallbackTarget_ = cb_target;
  memset(compName, 0x00, 64);
  sprintf(compName, "\"MHALVideoDecoder\"");
  mNumPorts_ = 2;
  OMX_LOGD("EXIT");
}

MHALVideoDecoder::~MHALVideoDecoder(){
  OMX_LOGD("ENTER");
  freeH264ConfigData();
  OMX_LOGD("EXIT");
}

const char* MHALVideoDecoder::getComponentRole() {
  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return NULL;
  }

  const char* mime_type = NULL;
  caps->GetString("mime_type", &mime_type);

  if (mime_type) {
    for (int i = 0; i < NELEM(kVideoCodecTable); ++i) {
      if (!strcmp(mime_type, kVideoCodecTable[i].mime_type_)) {
        OMX_LOGD("find role %s", kVideoCodecTable[i].decoderRole);
        return kVideoCodecTable[i].decoderRole;
      }
    }
  }

  OMX_LOGD("no role found");
  return NULL;
}

OMX_BOOL MHALVideoDecoder::configPort() {
  OMX_LOGD("ENTER");

  OMX_ERRORTYPE ret = OMX_ErrorNone;
  const char* mime_type = NULL;
  mint32 width = 0, height = 0, profile = 0, level = 0, framerate = 0;

  if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    MmpPad* video_src_pad = new MmpPad("video_src",
        MmpPad::MMP_PAD_SRC, MmpPad::MMP_PAD_MODE_PUSH);
    video_src_pad->SetOwner(this);
    AddPad(video_src_pad);
  }

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return OMX_FALSE;
  }

  caps->GetString("mime_type", &mime_type);
  caps->GetInt32("width", &width);
  caps->GetInt32("height", &height);
  caps->GetInt32("profile", &profile);
  caps->GetInt32("level", &level);
  caps->GetInt32("frame_rate", &framerate);

#ifdef CHECKING_H264_PROFILE
  if (!strcmp(mime_type, MMP_MIMETYPE_VIDEO_AVC)) {
    if (!isSupportedH264Profile(profile)) {
      OMX_LOGE("H264 profile %d is not supported!", profile);
      return OMX_FALSE;
    }
  }
#endif

  OMX_PORT_PARAM_TYPE oParam;
  InitOmxHeader(&oParam);
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamVideoInit, &oParam);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamVideoInit) with error %d", ret);
    return OMX_FALSE;
  }
  OMX_LOGD("oParam.nStartPortNumber = %d, oParam.nPorts = %d",
      oParam.nStartPortNumber, oParam.nPorts);

  if (!strcmp(mime_type, MMP_MIMETYPE_VIDEO_RV)) {
    setRVFormat(oParam.nStartPortNumber);
  }

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxHeader(&def);
  def.nPortIndex = oParam.nStartPortNumber;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamPortDefinition) with error %d", ret);
    return OMX_FALSE;
  }

  def.format.video.nFrameWidth = width;
  def.format.video.nFrameHeight = height;
  /**************************************************************************************
     video frame rate must be passed in to OMX, so that the correct frame rate can be set
     to video decoder to caculate video PTS in case PTS is not carried with frame data.
     in some streams, avstream->avg_frame_rate is invalid, so use avstream->r_frame_rate:
     frame rate = r_frame_rate.num/avstream->r_frame_rate.den
     = def.format.video.xFramerate / 0x10000
   ***************************************************************************************/
  def.format.video.xFramerate = framerate;
  OMX_LOGI("video.nFrameWidth = %d, video.nFrameHeight = %d, xFramerate: %d",
      def.format.video.nFrameWidth, def.format.video.nFrameHeight,
      def.format.video.xFramerate);
  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamPortDefinition) with error %d", ret);
    return OMX_FALSE;
  }

  if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    // Set up video output port.
    def.nPortIndex = oParam.nStartPortNumber + 1;
    ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamPortDefinition) with error %d", ret);
      return OMX_FALSE;
    }
    def.format.video.nFrameWidth = width;
    def.format.video.nFrameHeight = height;
    ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamPortDefinition) with error %d", ret);
      return OMX_FALSE;
    }

    if (!enableGraphicBuffers(oParam.nStartPortNumber + 1, OMX_TRUE)) {
      OMX_LOGE("Failed to enalbe GraphicBuffer from native window for video output!");
      return OMX_FALSE;
    }
  }

  OMX_LOGD("EXIT");
  return OMX_TRUE;
}

void MHALVideoDecoder::setRVFormat(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mint32 rv_type = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }

  caps->GetInt32("rv_type", &rv_type);

  OMX_VIDEO_PARAM_RVTYPE rv;
  InitOmxHeader(&rv);
  rv.nPortIndex = inputPortIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamVideoRv, &rv);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamVideoRv) with error %d", ret);
    return;
  }

  // From http://en.wikipedia.org/wiki/RealVideo:
  //     rv30: RealVideo 8
  //     rv40: RealVideo 9, RealVideo 10
  // But now, just map rv40 to RealVideo 9,
  // because OMX_VIDEO_RVFORMATTYPE doesn't define RealVideo 10.
  if (8 == rv_type) {
    rv.eFormat = OMX_VIDEO_RVFormat8;
  } else if (9 == rv_type) {
    rv.eFormat = OMX_VIDEO_RVFormat9;
  }

  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamVideoRv, &rv);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamVideoRv) with error %d", ret);
    return;
  }

  OMX_LOGD("EXIT");
}

void MHALVideoDecoder::setVP6Format(const OMX_U32 inputPortIdx) {
  OMX_LOGD("ENTER");

  OMX_LOGD("EXIT");
}

void MHALVideoDecoder::pushH264ConfigData() {
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  const void *extradata = NULL;
  mint32 extradata_size = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetData("codec-specific-data", &extradata, &extradata_size);

  uint8_t *data = (uint8_t *)extradata;
  int size = extradata_size;
  if (extradata_size < 7) {
    OMX_LOGE("AVCC cannot be smaller than 7, extradata_size = %d.",
        extradata_size);
    return;
  }
  if (data[0] != 1u) {
    OMX_LOGE("We only support configurationVersion 1, but this is %u.", data[0]);
    return;
  }

  int lengthSize = 1 + (data[4] & 3);
  int numSeqParameterSets = data[5] & 31;
  data += 6;
  size -= 6;
  static const int kNALHeaderLength = 4;
  static const uint8_t kNALHeader[kNALHeaderLength] =
          {0x00, 0x00, 0x00, 0x01};
  OMX_LOGE("numSeqParameterSets = %d", numSeqParameterSets);
  for (int i = 0; i < numSeqParameterSets; ++i) {
      CHECK_GE(size, static_cast<size_t>(2));
      int length = U16_AT(data);
      data += 2;
      size -= 2;
      CHECK_GE(size, length);

      uint8_t *buffer = new uint8_t[length + kNALHeaderLength];
      CHECK(NULL != buffer);
      memcpy(buffer, kNALHeader, kNALHeaderLength);
      memcpy(buffer + kNALHeaderLength, data, length);
      // Put codec config data into list.
      Config_buffer *config_buffer = new Config_buffer;
      config_buffer->data = buffer;
      config_buffer->size = length + kNALHeaderLength;
      mH264_config_buffer.push_back(config_buffer);

      data += length;
      size -= length;
  }
  int numPictureParameterSets = *data;
  ++data;
  --size;
  OMX_LOGD("numPictureParameterSets = %d", numPictureParameterSets);
  for (int i = 0; i < numPictureParameterSets; ++i) {
      int length = U16_AT(data);
      data += 2;
      size -= 2;
      CHECK_GE(size, length);

      uint8_t *buffer = new uint8_t[length + kNALHeaderLength];
      CHECK(NULL != buffer);
      memcpy(buffer, kNALHeader, kNALHeaderLength);
      memcpy(buffer + kNALHeaderLength, data, length);
      // Put codec config data into list.
      Config_buffer *config_buffer = new Config_buffer;
      config_buffer->data = buffer;
      config_buffer->size = length + kNALHeaderLength;
      mH264_config_buffer.push_back(config_buffer);

      data += length;
      size -= length;
  }

  kdThreadMutexLock(mInBufferLock_);
  OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers_.begin());

  int data_len = 0;
  OMX_LOGD("mH264_config_buffer.size() = %d", mH264_config_buffer.size());
  vector<Config_buffer *>::iterator iter = mH264_config_buffer.begin();
  for (int i = 0; i < mH264_config_buffer.size(); i++) {
    Config_buffer *config_data = *iter;
    kdMemcpy(buf->pBuffer + data_len, config_data->data, config_data->size);
    OMX_LOGD("config_data->size = %d", config_data->size);
    data_len += config_data->size;

    iter++;
  }
  OMX_LOGD("data_len = %d", data_len);
  buf->nFilledLen = data_len;
  buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
  ret = OMX_EmptyThisBuffer(mCompHandle_, buf);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("OMX_EmptyThisBuffer() failed with error %d", ret);
  }
  mInputBuffers_.erase(mInputBuffers_.begin());
  kdThreadMutexUnlock(mInBufferLock_);
}


void MHALVideoDecoder::freeH264ConfigData() {
  vector<Config_buffer *>::iterator iter = mH264_config_buffer.begin();
  for (int i = 0; i < mH264_config_buffer.size(); i++) {
    Config_buffer *config_data = *iter;
    delete config_data->data;
    mH264_config_buffer.erase(iter);
    delete config_data;
  }
}


void MHALVideoDecoder::pushRVConfigData() {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  const void *extradata = NULL;
  mint32 extradata_size = 0, width = 0, height = 0, framerate_num = 0, framerate_den = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetInt32("width", &width);
  caps->GetInt32("height", &height);
  caps->GetInt32("frame_rate_num", &framerate_num);
  caps->GetInt32("frame_rate_den", &framerate_den);
  caps->GetData("codec-specific-data", &extradata, &extradata_size);

  RmPacketHeader* common_hdr = new RmPacketHeader;
  if (!common_hdr) {
    OMX_LOGE("Out of memory!");
    return;
  }
  RmStreamPayHeader* seq_header = new RmStreamPayHeader;
  if (!seq_header) {
    OMX_LOGE("Out of memory!");
    return;
  }

  uint8_t* extra_data = (uint8_t*)extradata;
  uint32_t extra_data_size = extradata_size;
  uint32_t frame_rate_ = framerate_num / framerate_den;

  seq_header->pic_dis_width = ((width & 0xff) << 8) + (width >> 8);
  seq_header->pic_dis_height = ((height & 0xff) << 8) + (height >> 8);
  seq_header->frame_rate = ((frame_rate_ & 0xff) << 24)
      + ((frame_rate_ & 0xff00) << 8)
      + ((frame_rate_ & 0xff0000) >> 8)
      + (frame_rate_ >> 24);

  if (extra_data_size >= 8) {
    seq_header->spo_flags = (extra_data[3] << 24) | (extra_data[2] <<16) |
        (extra_data[1] << 8) | extra_data[0];
    seq_header->bitstream_version = (extra_data[7] << 24) | (extra_data[6] <<16) |
        (extra_data[5] << 8) | extra_data[4];
  } else {
    seq_header->spo_flags = 0;
    seq_header->bitstream_version = 0;
  }

  if (extra_data_size >= 12) {
    seq_header->encode_size = (extra_data[11] << 24) | (extra_data[10] <<16) |
        (extra_data[9] << 8) | extra_data[8];
  } else {
    seq_header->encode_size = 0;
  }

  if (extra_data_size >= 14) {
    seq_header->resampled_image_width = (extra_data[13] << 8) | extra_data[12];
  } else {
    seq_header->resampled_image_width = 0;
  }
  seq_header->resampled_image_height = 0;

  common_hdr->start_code_1 = 0;
  common_hdr->start_code_2 = 1;
  common_hdr->packet_type = 0x10;
  common_hdr->reserved = 0;
  common_hdr->marker_byte_1 = 0x88;
  common_hdr->packet_payload_len_msb = 0;
  common_hdr->marker_byte_2 = 0x88;
  common_hdr->packet_payload_len_lsb = 0x1800;
  common_hdr->marker_byte_3 = 0x88;
  common_hdr->packet_padding_len = 0;
  common_hdr->marker_byte_4 = 0x88;

  kdThreadMutexLock(mInBufferLock_);
  OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers_.begin());
  if (buf && buf->pBuffer) {
    kdMemcpy(buf->pBuffer, common_hdr, sizeof(RmPacketHeader));
    kdMemcpy(buf->pBuffer + sizeof(RmPacketHeader), seq_header, sizeof(RmStreamPayHeader));
    buf->nFilledLen = sizeof(RmPacketHeader) + sizeof(RmStreamPayHeader);
    buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
    ret = OMX_EmptyThisBuffer(mCompHandle_, buf);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("OMX_EmptyThisBuffer() failed with error %d", ret);
    }
    mInputBuffers_.erase(mInputBuffers_.begin());
  }
  kdThreadMutexUnlock(mInBufferLock_);

  delete common_hdr;
  delete seq_header;
  OMX_LOGD("EXIT");
}


void MHALVideoDecoder::pushConfigData() {
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  const void *extradata = NULL;
  mint32 extradata_size = 0;

  MmpCaps* caps = getPadCaps();
  if (!caps) {
    OMX_LOGE("Can't get MmpCaps!");
    return;
  }
  caps->GetData("codec-specific-data", &extradata, &extradata_size);

  if (extradata_size > 0) {
    kdThreadMutexLock(mInBufferLock_);
    OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers_.begin());
    if (buf && buf->pBuffer) {
      kdMemcpy(buf->pBuffer, extradata, extradata_size);
      buf->nFilledLen = extradata_size;
      buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
      ret = OMX_EmptyThisBuffer(mCompHandle_, buf);
      if (OMX_ErrorNone != ret) {
        OMX_LOGE("OMX_EmptyThisBuffer() failed with error %d", ret);
      }
      mInputBuffers_.erase(mInputBuffers_.begin());
    }
    kdThreadMutexUnlock(mInBufferLock_);
  }
}


void MHALVideoDecoder::pushCodecConfigData() {
  OMX_LOGD("ENTER");
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

  uint8_t *data = (uint8_t *)extradata;
  if (!strcmp(mime_type, MMP_MIMETYPE_VIDEO_AVC) &&
      (extradata_size >= 7) && (1u == data[0])) {
    pushH264ConfigData();
  } else if (!strcmp(mime_type, MMP_MIMETYPE_VIDEO_RV) && (extradata_size >= 0)) {
    pushRVConfigData();
  } else if (extradata_size > 0){
    pushConfigData();
  }
  OMX_LOGD("EXIT");
}

OMX_BOOL MHALVideoDecoder::queueEOSToOMX() {
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

OMX_BOOL MHALVideoDecoder::queueBufferToOMX(MmpBuffer* pkt) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  if (!pkt) {
    OMX_LOGE("pkt is NULL!");
    return OMX_FALSE;
  }

  // Some *.ts streams does not start with I-Frame.
  // Don't send these frames to video pipeline to avoid mosaic.
  if (bWaitVideoIFrame_ && !pkt->IsFlagSet(MmpBuffer::MMP_BUFFER_FLAG_KEY_FRAME)) {
    OMX_LOGE("Waiting for I frame come, in order to avoid mosaic.");
    return OMX_TRUE;
  } else if (bWaitVideoIFrame_ && pkt->IsFlagSet(MmpBuffer::MMP_BUFFER_FLAG_KEY_FRAME)) {
    OMX_LOGD("I frame comes, good!");
    bWaitVideoIFrame_ = OMX_FALSE;
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
    OMX_LOGD("no enough buffer, require %d but mInputBuffers_.size() is %d, "
        "sleep 10ms and try again", buffers_need, mInputBuffers_.size());
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
    if (pkt->size - bytes_sent > buf->nAllocLen) {
      // TODO: should send less than buf->nAllocLen,
      // because OMX IL may merge codec config data
      kdMemcpy(buf->pBuffer, pkt->data + bytes_sent, buf->nAllocLen);
      buf->nFilledLen = buf->nAllocLen;
      buf->nTimeStamp = pkt->pts;
      bytes_sent += buf->nFilledLen;
    } else {
      kdMemcpy(buf->pBuffer, pkt->data + bytes_sent, pkt->size - bytes_sent);
      buf->nFilledLen = pkt->size - bytes_sent;
      buf->nTimeStamp = pkt->pts;
      bytes_sent += buf->nFilledLen;
      if (pkt->size > buf->nAllocLen) {
        buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
      }
    }
    if (pkt->IsFlagSet(MmpBuffer::MMP_BUFFER_FLAG_KEY_FRAME)) {
      buf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
    }
    OMX_LOGD("Push frame, buf = %p, pts = "TIME_FORMAT"(%lld us), size = %lu, flags = 0x%x",
        buf, TIME_ARGS(buf->nTimeStamp), buf->nTimeStamp, buf->nFilledLen, buf->nFlags);
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
OMX_BOOL MHALVideoDecoder::processData() {
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

#ifdef CHECKING_H264_PROFILE
OMX_BOOL MHALVideoDecoder::isSupportedH264Profile(int profile) {
  // According to the spec, vMeta would take the syntax profile as an 8-bit unsigned char.
  uint8_t h264_profile = profile & 0xFF;

  if ((h264_profile == PROFILE_H264_BASELINE) ||
      (h264_profile == PROFILE_H264_MAIN) ||
      (h264_profile == PROFILE_H264_EXTENDED) ||
      (h264_profile == PROFILE_H264_HIGH) ||
      (h264_profile == PROFILE_H264_MULTIVIEW_HIGH) ||
      (h264_profile == PROFILE_H264_STEREO_HIGH)) {
    return OMX_TRUE;
  }

  OMX_LOGE("H264 profile %d is not supported", profile);
  return OMX_FALSE;
}
#endif

OMX_ERRORTYPE MHALVideoDecoder::OnOMXEvent(OMX_EVENTTYPE eEvent,
    OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData) {
  switch (eEvent) {
// Disable it to pass CTS test.
#if 0
    case OMX_EventVideoResolutionChange:
      OMX_LOGI("event = OMX_EventVideoResolutionChange, new res = %d x %d", nData1, nData2);
      if (mCallbackTarget_) {
        mCallbackTarget_->OnOmxEvent(EVENT_VIDEO_RESOLUTION, nData1, nData2);
      }
      break;
#endif

    case OMX_EventBufferFlag:
      OMX_LOGI("event = OMX_EventBufferFlag, data1 = %lu, data2 = %lu", nData1, nData2);
      if ((nData2 & OMX_BUFFERFLAG_EOS) != 0) {
        OMX_LOGI("EOS received!!!");
        mOutPortEOS_ = OMX_TRUE;
        if ((OMX_NON_TUNNEL == omx_tunnel_mode_) && mCallbackTarget_) {
          mCallbackTarget_->OnOmxEvent(EVENT_VIDEO_EOS);
        }
      }
      break;

    default:
      return MHALOmxComponent::OnOMXEvent(eEvent, nData1, nData2, pEventData);
  }

  return OMX_ErrorNone;
}

void MHALVideoDecoder::fillThisBuffer(MmpBuffer *buf) {
  // Secondly, get an empty buffer from native window and let OMX fill it.
  if (!mOutPortEOS_) {
    // Find a buffer and push to OMX.
    BufferInfo *next_buf = dequeueBufferFromNativeWindow();
    if (next_buf) {
      OMX_FillThisBuffer(mCompHandle_, next_buf->header);
    }
  }
}

OMX_ERRORTYPE MHALVideoDecoder::FillBufferDone(OMX_BUFFERHEADERTYPE* pBuffer) {
  // Firstly, push this buffer to video render.
  for (OMX_U32 j = 0; j < mOutputBuffers_.size(); j++) {
    BufferInfo *info = mOutputBuffers_[j];
    if (info && info->header == pBuffer) {
      MmpPad* src_pad = GetSrcPadByPrefix("video_src", 0);
      if (src_pad) {
        info->buffer->pts = pBuffer->nTimeStamp;
        src_pad->Push(info->buffer);
      }
    }
  }

  return OMX_ErrorNone;
}

}
