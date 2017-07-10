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

#include <FFMpegVideoDecoder.h>
#ifdef _ANDROID_
#include <media/hardware/HardwareAPI.h>
#endif

#define LOG_TAG "FFMpegVideoDecoder"

namespace mmp {

//#define DUMP_OUTPUT
#ifdef DUMP_OUTPUT
  FILE *fd_output = NULL;
#endif

static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

static int min(int x, int y) {
    return (x > y) ? y : x;
}

FFMpegVideoDecoder::FFMpegVideoDecoder() :
    mAVCodec(NULL),
    mAVCodecContext(NULL),
    mAVFrame(NULL),
    mSwsContext(NULL),
    mColorFormat(PIX_FMT_YUV420P),
    mSwapUV(OMX_FALSE) {
  OMX_LOGD("");
}

FFMpegVideoDecoder::~FFMpegVideoDecoder() {
  deinit();
  OMX_LOGD("");
}

OMX_ERRORTYPE FFMpegVideoDecoder::init(OMX_VIDEO_CODINGTYPE type, OMX_COLOR_FORMATTYPE color_format,
    OMX_U8* codec_data, OMX_U32 data_size) {
  enum AVCodecID target_codecID;
  av_register_all();
  OMX_LOGD("avcodec initialized");
  switch(type) {
    case OMX_VIDEO_CodingMPEG4:
      OMX_LOGD("Mpeg4");
      target_codecID = CODEC_ID_MPEG4;
      break;
    case OMX_VIDEO_CodingAVC:
      OMX_LOGD("H264");
      target_codecID = CODEC_ID_H264;
      break;
//#ifdef _ANDROID_
//    case OMX_VIDEO_CodingVPX:
//      target_codecID = CODEC_ID_VP8;
//      break;
//#endif
    default :
      OMX_LOGE("codecs %d not supported", type);
      return OMX_ErrorComponentNotFound;
  }

  switch(color_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
      mColorFormat = AV_PIX_FMT_RGBA;
      OMX_LOGD("HAL_PIXEL_FORMAT_RGBA_8888 -> AV_PIX_FMT_RGBA");
      break;
    case HAL_PIXEL_FORMAT_YV12:
      mColorFormat = AV_PIX_FMT_YUV420P;
      mSwapUV = OMX_TRUE;
      OMX_LOGD("HAL_PIXEL_FORMAT_YV12 -> AV_PIX_FMT_YUV420P, should swap UV");
      break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
      mColorFormat = AV_PIX_FMT_YUV422P;
      OMX_LOGD("HAL_PIXEL_FORMAT_YCbCr_422_I -> AV_PIX_FMT_YUV422P");
      break;
    case OMX_COLOR_FormatYUV420Planar:
      mColorFormat = AV_PIX_FMT_YUV420P;
      OMX_LOGD("OMX_COLOR_FormatYUV420Planar -> AV_PIX_FMT_YUV420P");
      break;
    default :
      OMX_LOGW("color_format %d not found, use AV_PIX_FMT_UYVY422", color_format);
      mColorFormat = AV_PIX_FMT_UYVY422;
      break;
  }

  mAVCodec = avcodec_find_decoder(target_codecID);
  if (mAVCodec == NULL) {
    OMX_LOGE("Codec not found");
    return OMX_ErrorInsufficientResources;
  }

  OMX_LOGD("Extra data size %d", data_size);
  mAVCodecContext = avcodec_alloc_context3(mAVCodec);
  if (!mAVCodecContext) {
    OMX_LOGE("avcodec_alloc_context3() failed");
    return OMX_ErrorInsufficientResources;
  }

  if(data_size > 0) {
    mAVCodecContext->extradata = codec_data;
    mAVCodecContext->extradata_size = (int)data_size;
  } else {
    //mAVCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  }

  if (mAVCodec->capabilities & CODEC_CAP_FRAME_THREADS) {
    OMX_LOGD("codec support CODEC_CAP_FRAME_THREADS, enable multi-threads");
    mAVCodecContext->thread_count = 4;
    mAVCodecContext->active_thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    OMX_LOGD("mAVCodecContext->thread_count = %d, mAVCodecContext->active_thread_type = 0x%x",
        mAVCodecContext->thread_count, mAVCodecContext->active_thread_type);
  }

  int ret = avcodec_open2(mAVCodecContext, mAVCodec, NULL);
  if (ret < 0) {
    OMX_LOGE("Could not open codec with error %d", ret);
    return OMX_ErrorInsufficientResources;
  }

  OMX_LOGD("mAVCodecContext->codec->capabilities = 0x%x, mAVCodecContext->flags = 0x%x, "
      "mAVCodecContext->flags2 = 0x%x, mAVCodecContext->thread_count = %d, "
      "mAVCodecContext->thread_type = 0x%x, mAVCodecContext->active_thread_type = 0x%x",
      mAVCodecContext->codec->capabilities, mAVCodecContext->flags,
      mAVCodecContext->flags2, mAVCodecContext->thread_count,
      mAVCodecContext->thread_type, mAVCodecContext->active_thread_type);

  mAVFrame = avcodec_alloc_frame();
  if (!mAVFrame) {
    OMX_LOGE("Could not get AVFrame");
    return OMX_ErrorInsufficientResources;
  }

  mWidth = mAVCodecContext->width;
  mHeight = mAVCodecContext->height;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE FFMpegVideoDecoder::deinit() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mAVCodecContext) {
    avcodec_close(mAVCodecContext);
    av_free (mAVCodecContext);
    mAVCodecContext = NULL;
  }
  if (mAVFrame) {
    av_free(mAVFrame);
    mAVFrame = NULL;
  }
  if (mSwsContext) {
    sws_freeContext(mSwsContext);
    mSwsContext = NULL;
  }

#ifdef DUMP_OUTPUT
  if (fd_output) {
    fclose(fd_output);
    fd_output = NULL;
  }
#endif

  return err;
}

OMX_ERRORTYPE FFMpegVideoDecoder::flush() {
  if (mAVCodecContext) {
    avcodec_flush_buffers(mAVCodecContext);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE FFMpegVideoDecoder::decode(OMX_U8 *in_buf, OMX_U32 in_size,
    OMX_TICKS in_pts, OMX_U8 *out_buf, OMX_U32 *out_size,
    OMX_U32 *in_consume, OMX_TICKS *pts) {
  OMX_LOGD("ENTER");
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int got_output = 0, ret = 0;
  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = in_buf;
  avpkt.size = in_size;
  avpkt.dts = in_pts;

  avcodec_get_frame_defaults(mAVFrame);
  while (avpkt.size > 0) {
    ret = avcodec_decode_video2(mAVCodecContext, mAVFrame, &got_output, &avpkt);
    if (ret < 0) {
      OMX_LOGE("A general error or simply frame not decoded? ret = %d", ret);
      return OMX_ErrorInsufficientResources;
    }
    OMX_LOGD("%d bytes are decoded out of %d input bytes, got_output = %d",
        ret, in_size, got_output);
    avpkt.data += ret;
    avpkt.size -= ret;
    *in_consume += ret;
  }

  if (mWidth != mAVCodecContext->width ||
      mHeight != mAVCodecContext->height) {
    OMX_LOGI("Resolution changed, %d x %d -> %d x%d",
        mWidth, mHeight, mAVCodecContext->width, mAVCodecContext->height);
    mWidth = mAVCodecContext->width;
    mHeight = mAVCodecContext->height;
  }

  if (got_output) {
    AVPicture pic;
    *out_size = avpicture_get_size (mColorFormat, mWidth, mHeight);
    avpicture_fill (&pic, out_buf, mColorFormat, mWidth, mHeight );
    OMX_LOGD("Decode output size %d, ts %lld,", *out_size, mAVFrame->best_effort_timestamp);
    if (!mSwsContext) {
      mSwsContext = sws_getContext( mAVCodecContext->width,
          mAVCodecContext->height,
          mAVCodecContext->pix_fmt,
          mAVCodecContext->width, // TODO: dst width, when using native buffer, no scaling needed?
          mAVCodecContext->height, // dst height
          mColorFormat, // dstFormat
          SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }
    if (mSwapUV) {
      uint8_t *temp = pic.data[1];
      pic.data[1] = pic.data[2];
      pic.data[2] = temp;
    }
    sws_scale(mSwsContext, mAVFrame->data,
        mAVFrame->linesize, 0,
        mAVCodecContext->height, pic.data, pic.linesize );
    *pts = mAVFrame->best_effort_timestamp;
  }

  OMX_LOGD("EXIT success");
  return err;
}

OMX_ERRORTYPE FFMpegVideoDecoder::decode(OMX_U8 *in_buf, OMX_U32 in_size,
    OMX_TICKS in_pts, OMX_U32 *in_consume, OMX_U32 *got_decoded_frame) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int got_output = 0, ret = 0;
  AVPacket avpkt;

//#define COUNTING_PERFORMANCE
#ifdef COUNTING_PERFORMANCE
  static int frames_decoded = 0;
  static OMX_BOOL is_playing = OMX_FALSE;
  static struct timeval last;
  struct timeval current;
  float time_past = 0.0;

  if (!is_playing) {
    OMX_LOGD("start counting performance");
    is_playing = OMX_TRUE;
    gettimeofday(&last, NULL);
  }
#endif

  av_init_packet(&avpkt);
  avpkt.data = in_buf;
  avpkt.size = in_size;
  avpkt.pts = in_pts;

  avcodec_get_frame_defaults(mAVFrame);
  while (avpkt.size > 0) {
    ret = avcodec_decode_video2(mAVCodecContext, mAVFrame, &got_output, &avpkt);
    if (ret < 0) {
      OMX_LOGE("A general error or simply frame not decoded? ret = %d", ret);
      return OMX_ErrorInsufficientResources;
    }
    //OMX_LOGD("%d bytes are decoded out of %d input bytes, got_output = %d",
    //    ret, in_size, got_output);
    avpkt.data += ret;
    avpkt.size -= ret;
    *in_consume += ret;
    *got_decoded_frame = got_output;
  }

#ifdef COUNTING_PERFORMANCE
  if (got_output) {
    frames_decoded++;
    gettimeofday(&current, NULL);
    time_past = ((current.tv_sec - last.tv_sec) * 1000000 + (current.tv_usec - last.tv_usec))
        / (float)1000000;
    if (time_past >= 1.0) {
      OMX_LOGD("%d frames decoded in the past %f s, fps is %f",
          frames_decoded, time_past, frames_decoded / time_past);
      // reset for next count
      frames_decoded = 0;
      last.tv_sec  = current.tv_sec;
      last.tv_usec = current.tv_usec;
    }
  }
#endif

  if (mWidth != mAVCodecContext->width ||
      mHeight != mAVCodecContext->height) {
    OMX_LOGI("Resolution changed, %d x %d -> %d x%d",
        mWidth, mHeight, mAVCodecContext->width, mAVCodecContext->height);
    mWidth = mAVCodecContext->width;
    mHeight = mAVCodecContext->height;
  }

  return err;
}


OMX_ERRORTYPE FFMpegVideoDecoder::fetchDecodedFrame(
    OMX_U8 *out_buf, OMX_U32 *out_size, OMX_TICKS *pts) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int filledBytes = 0;

  if ((mColorFormat == mAVCodecContext->pix_fmt) && (mColorFormat == AV_PIX_FMT_YUV420P)) {
    if  (mSwapUV) {
      uint8_t *src_Y = mAVFrame->data[0];
      for (int i = 0; i < mHeight; i++) {
        memcpy(out_buf, src_Y, mWidth);
        out_buf += mWidth;
        src_Y += mAVFrame->linesize[0];
        filledBytes += mWidth;
      }

      int dst_c_stride = ALIGN(mWidth, 32) / 2;
      uint8_t *src_Cr = mAVFrame->data[2];
      for (int i = 0; i < mHeight / 2; i++) {
        memcpy(out_buf, src_Cr, min(dst_c_stride, mAVFrame->linesize[2]));
        out_buf += dst_c_stride;
        src_Cr += mAVFrame->linesize[2];
        filledBytes += min(dst_c_stride, mAVFrame->linesize[2]);
      }

      uint8_t *src_Cb = mAVFrame->data[1];
      for (int i = 0; i < mHeight / 2; i++) {
        memcpy(out_buf, src_Cb, min(dst_c_stride, mAVFrame->linesize[1]));
        out_buf += dst_c_stride;
        src_Cb += mAVFrame->linesize[1];
        filledBytes += min(dst_c_stride, mAVFrame->linesize[1]);
      }

      *out_size = filledBytes;
    } else {
      *out_size = avpicture_get_size(mColorFormat, mWidth, mHeight);
      avpicture_layout((AVPicture *)mAVFrame, mColorFormat, mWidth, mHeight, out_buf, *out_size);
    }
  }else {
    AVPicture pic;
    avpicture_fill(&pic, out_buf, mColorFormat, mWidth, mHeight);
    if (!mSwsContext) {
      mSwsContext = sws_getContext(mAVCodecContext->width,
          mAVCodecContext->height,
          mAVCodecContext->pix_fmt,
          mAVCodecContext->width, // TODO: dst width, when using native buffer, no scaling needed?
          mAVCodecContext->height, // dst height
          mColorFormat, // dstFormat
          SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }

    if (mSwapUV) {
      uint8_t *temp = pic.data[1];
      pic.data[1] = pic.data[2];
      pic.data[2] = temp;
    }

    sws_scale(mSwsContext, mAVFrame->data,
        mAVFrame->linesize, 0,
        mAVCodecContext->height, pic.data, pic.linesize);

    *out_size = avpicture_get_size(mColorFormat, mWidth, mHeight);
  }

  *pts = av_frame_get_best_effort_timestamp(mAVFrame);

#ifdef DUMP_OUTPUT
  if (!fd_output) {
    fd_output = fopen("/data/video_output.dat", "w+");
  }
  if (fd_output) {
    fwrite(out_buf, 1, *out_size, fd_output);
    fflush(fd_output);
  }
#endif

  return err;
}

}
