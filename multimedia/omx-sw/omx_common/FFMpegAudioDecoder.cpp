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

#include <FFMpegAudioDecoder.h>

#define LOG_TAG "FFMpegAudioDecoder"

namespace mmp {

FFMpegAudioDecoder::FFMpegAudioDecoder() :
    mAVCodec(NULL),
    mAVCodecContext(NULL),
    mAVFrame(NULL),
    mSwrContext(NULL) {
}

FFMpegAudioDecoder::~FFMpegAudioDecoder() {
  deinit();
  OMX_LOGD("");
}

OMX_ERRORTYPE FFMpegAudioDecoder::init(OMX_AUDIO_CODINGTYPE type,
    OMX_U32 bitsPerSample, OMX_U32 channels, OMX_U32 sampleRate,
    OMX_U8* codec_data, OMX_U32 data_size) {
  av_register_all();

  mOutChannels = channels;
  mOutSampleRate = sampleRate;
  if (bitsPerSample == 8) {
    mOutSampleFormat = AV_SAMPLE_FMT_U8;
  } else if (bitsPerSample == 16) {
    mOutSampleFormat = AV_SAMPLE_FMT_S16;
  } else if (bitsPerSample == 32) {
    mOutSampleFormat = AV_SAMPLE_FMT_S32;
  }

  enum AVCodecID target_codecID = AV_CODEC_ID_NONE;
  switch(type) {
    case OMX_AUDIO_CodingMP3:
      target_codecID = AV_CODEC_ID_MP3;
      OMX_LOGE("mp3");
      break;
    case OMX_AUDIO_CodingAAC:
      target_codecID = AV_CODEC_ID_AAC;
      OMX_LOGE("aac");
      break;
    case OMX_AUDIO_CodingVORBIS:
      target_codecID = AV_CODEC_ID_VORBIS;
      OMX_LOGE("vorbis");
      break;
    default:
      OMX_LOGE("codecs %d not supported", type);
      return OMX_ErrorComponentNotFound;
  }

  mAVCodec = avcodec_find_decoder(target_codecID);
  if (mAVCodec == NULL) {
    OMX_LOGE("Codec not found");
    return OMX_ErrorInsufficientResources;
  }

  mAVCodecContext = avcodec_alloc_context3(mAVCodec);
  if (!mAVCodecContext) {
    OMX_LOGE("CodecContext is not found");
    return OMX_ErrorInsufficientResources;
  }

  if(data_size > 0) {
    OMX_LOGD("Extra data size %d", data_size);
    mAVCodecContext->extradata = codec_data;
    mAVCodecContext->extradata_size = (int)data_size;
  }

  int ret = avcodec_open2(mAVCodecContext, mAVCodec, NULL);
  if (ret < 0) {
    OMX_LOGE("Could not open codec with error %d", ret);
    return OMX_ErrorInsufficientResources;
  }

  mAVFrame = avcodec_alloc_frame();
  if (!mAVFrame) {
    OMX_LOGE("Could not get AVFrame");
    return OMX_ErrorInsufficientResources;
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE FFMpegAudioDecoder::deinit() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mAVCodecContext) {
    avcodec_close(mAVCodecContext);
    av_free(mAVCodecContext);
    mAVCodecContext = NULL;
  }
  if(mAVFrame) {
    av_free(mAVFrame);
    mAVFrame = NULL;
  }
  if (mSwrContext) {
    swr_free(&mSwrContext);
  }
  return err;
}

OMX_ERRORTYPE FFMpegAudioDecoder::flush() {
  if (mAVCodecContext) {
    avcodec_flush_buffers(mAVCodecContext);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE FFMpegAudioDecoder::decode(OMX_U8 *in_buf, OMX_U32 in_size, OMX_TICKS in_pts,
    OMX_U8 *out_buf, OMX_U32 *out_size, OMX_U32 *in_consume, OMX_TICKS *pts) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int got_output = 0, ret = 0;
  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = in_buf;
  avpkt.size = in_size;
  avpkt.pts = in_pts;

  *in_consume = 0;

  avcodec_get_frame_defaults(mAVFrame);
  while (avpkt.size > 0) {
    ret = avcodec_decode_audio4(mAVCodecContext, mAVFrame, &got_output, &avpkt);
    if (ret < 0) {
      OMX_LOGE("A general error or simply frame not decoded? ret = %d", ret);
      return OMX_ErrorInsufficientResources;
    }
    //OMX_LOGD("%d bytes are decoded out of %d input bytes", ret, in_size);
    avpkt.data += ret;
    avpkt.size -= ret;
    *in_consume += ret;
  }

  if (got_output) {
    if (!mSwrContext) {
      OMX_LOGD("expected output format: mOutSampleFormat = %d, mOutChannels = %d, "
          "mOutSampleRate = %d", mOutSampleFormat, mOutChannels, mOutSampleRate);
      mSwrContext = swr_alloc_set_opts(NULL,
          av_get_default_channel_layout(mOutChannels), mOutSampleFormat, mOutSampleRate,
          av_get_default_channel_layout(mAVFrame->channels),
          (AVSampleFormat)mAVFrame->format, mAVFrame->sample_rate,
          0, NULL);
      swr_init(mSwrContext);
    }

    if (mSwrContext) {
      int samples = swr_convert(mSwrContext, &out_buf, mAVFrame->nb_samples,
          (const uint8_t**)mAVFrame->data, mAVFrame->nb_samples);
      *out_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * samples * mAVFrame->channels;
    }

    //OMX_LOGD("Got %d bytes after decode, mAVFrame->linesize[0] = %d, out_size = %d",
    //    data_size, mAVFrame->linesize[0], *out_size);
    *pts = av_frame_get_best_effort_timestamp(mAVFrame);
  }

  return err;
}

}
