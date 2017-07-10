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

#include "MRVLFFMetadataRetriever.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#undef CodecType
}

#include <media/stagefright/ColorConverter.h>
#include <media/stagefright/foundation/ADebug.h>


#undef LOG_NDEBUG
#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "MRVLFFMetadataRetriever"

namespace android {

const char* const proc_link_url = "/proc/self/fd";
Mutex MRVLFFMetadataRetriever::lock_;

MediaMetadataRetrieverInterface* createMRVLFFMetadataRetriever() {
  return new MRVLFFMetadataRetriever();
}

MRVLFFMetadataRetriever::MRVLFFMetadataRetriever()
  : is_metadata_extracted_(false),
    init_(false),
    use_stagefright_(false),
    source_url_(NULL),
    context_(NULL),
    mRawDataSource_(NULL) {
    av_register_all();
    avcodec_register_all();
}

MRVLFFMetadataRetriever::~MRVLFFMetadataRetriever() {
  reset();
}


void MRVLFFMetadataRetriever::reset() {
  delete mRawDataSource_;
  if (context_) {
    avformat_close_input(&context_);
    context_ = NULL;
  }
  if (source_url_) {
    free(source_url_);
    source_url_ = NULL;
  }
  init_ = false;
}

status_t MRVLFFMetadataRetriever::setDataSource(const char *url,
    const KeyedVector<String8, String8> *headers) {

  AVInputFormat *fmt;
  status_t status = android::NO_ERROR;

  AutoMutex lock(lock_);
  if (init_) {
    reset();
  }
  int res;

  RETRIEVER_LOGD("Set data source %s", url);

  bool ret = true;
  mRawDataSource_ = new FileDataSource(url, &ret);
  AVInputFormat *input_format;

  if (false == ret) {
    RETRIEVER_LOGE("This file is no longer exist, we cannot open");
    if (android::OK == (status = StagefrightMetadataRetriever::setDataSource(url, NULL))) {
      use_stagefright_ = true;
      goto succeed;
  }
    return android::UNKNOWN_ERROR;
  }

  input_format = mRawDataSource_->getAVInputFormat();

  if (!input_format) {
    RETRIEVER_LOGE("Unknown media format by ffmpeg, try StagefrightMetadataRetriever.");
    if (android::OK == (status = StagefrightMetadataRetriever::setDataSource(url, NULL))) {
      use_stagefright_ = true;
      goto succeed;
    } else {
      RETRIEVER_LOGE("Unknown media format, "
          "can't be retrieved by StagefrightMetadataRetriever and ffmpeg.");
      return android::UNKNOWN_ERROR;
    }
  }

  if(!context_) {
    context_= avformat_alloc_context();
    if(!context_) {
      RETRIEVER_LOGE("avformat_alloc_context failed");
      return android::UNKNOWN_ERROR;
    }
  }
  context_->pb = (AVIOContext*)mRawDataSource_->getByteIOContext();
  res = avformat_open_input(&context_, "", input_format, NULL);
  if (res != 0) {
    RETRIEVER_LOGE("av_open_input_file %s failed %d", url, res);
    return android::UNKNOWN_ERROR;
  }
#if 0
  if((context_->nb_streams == 1) &&
      (context_->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO ||
      context_->streams[0]->codec->codec_id > AV_CODEC_ID_VP8)){
    ALOGE("\n%s,%d, incorrect format for thumbnail",__FUNCTION__,__LINE__);
    return android::UNKNOWN_ERROR;
  }
#endif
  res = av_find_stream_info(context_);
  if (res < 0) {
    RETRIEVER_LOGE("av_find_stream_info %s failed %d", url, res);
    avformat_close_input(&context_);
    context_ = NULL;
    return android::UNKNOWN_ERROR;
  }

  av_dump_format(context_, 0, 0, 0);

  if (!strcmp(context_->iformat->name, "mp3")
      || !strcmp(context_->iformat->name, "ogg")
      || !strcmp(context_->iformat->name, "flac")
      || !strcmp(context_->iformat->name, "mov,mp4,m4a,3gp,3g2,mj2")
      || (!strcmp(context_->iformat->name, "asf") && context_->nb_streams == 1)) {
    if (android::OK == (status = StagefrightMetadataRetriever::setDataSource(url, NULL))) {
      use_stagefright_ = true;
    }
  }

succeed:
  init_ = true;
  return android::OK;
}

status_t MRVLFFMetadataRetriever::setDataSource(int fd, int64_t offset, int64_t length) {
  AVInputFormat *fmt = NULL;
  bool ts_stream = false;

  // FFmpeg can't probe the contents if it's embedded in the middle of a file
  if (offset != 0) {
    RETRIEVER_LOGE("Giving up due to non-zero offset %lld", offset);
    return android::INVALID_OPERATION;
  }

  if (source_url_) {
    free(source_url_);
    source_url_ = NULL;
  }

  AutoMutex lock(lock_);
  if (init_) {
    reset();
  }
  int res;

  status_t status = android::NO_ERROR;

  mRawDataSource_ = new FileDataSource(fd, offset, length);

  AVInputFormat *input_format = mRawDataSource_->getAVInputFormat();

  if (!input_format) {
    RETRIEVER_LOGE("Unknown media format by ffmpeg, try StagefrightMetadataRetriever.");
    if (android::OK == (status = StagefrightMetadataRetriever::setDataSource(fd, offset, length))) {
      use_stagefright_ = true;
      goto succeed;
    } else {
      RETRIEVER_LOGE("Unknown media format, "
          "can't be retrieved by StagefrightMetadataRetriever and ffmpeg.");
      return android::UNKNOWN_ERROR;
    }
  }

  if(!context_) {
    context_= avformat_alloc_context();
    if(!context_) {
      RETRIEVER_LOGE("avformat_alloc_context failed");
      return android::UNKNOWN_ERROR;
    }
  }
  context_->pb = (AVIOContext*)mRawDataSource_->getByteIOContext();
  res = avformat_open_input(&context_, "", input_format, NULL);
  if (res != 0) {
    RETRIEVER_LOGE("av_open_input_file %d failed %d", fd, res);
    return android::UNKNOWN_ERROR;
  }
#if 0
  if((context_->nb_streams == 1) &&
      (context_->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO ||
      context_->streams[0]->codec->codec_id > AV_CODEC_ID_VP8)){
    ALOGE("\n%s,%d, incorrect format for thumbnail",__FUNCTION__,__LINE__);
    return android::UNKNOWN_ERROR;
  }
#endif
  res = av_find_stream_info(context_);
  if (res < 0) {
    RETRIEVER_LOGE("av_find_stream_info %d failed %d", fd, res);
    avformat_close_input(&context_);
    context_ = NULL;
    return android::UNKNOWN_ERROR;
  }

  av_dump_format(context_, 0, 0, 0);

  if (!strcmp(context_->iformat->name, "mp3")
      || !strcmp(context_->iformat->name, "ogg")
      || !strcmp(context_->iformat->name, "flac")
      || !strcmp(context_->iformat->name, "mov,mp4,m4a,3gp,3g2,mj2")
      || (!strcmp(context_->iformat->name, "asf") && context_->nb_streams == 1)) {
    if (android::OK == (status = StagefrightMetadataRetriever::setDataSource(fd, offset, length))) {
      use_stagefright_ = true;
    }
  }

succeed:
  init_ = true;
  return android::OK;
}

MediaAlbumArt* MRVLFFMetadataRetriever::extractAlbumArt() {
  if (use_stagefright_) {
    return StagefrightMetadataRetriever::extractAlbumArt();
  }
  // FFmpeg doesn't support extracting albumart from audio files.
  return NULL;
}

const char* MRVLFFMetadataRetriever::extractMetadata(int keyCode) {
  bool res = true;
  AutoMutex lock(lock_);

  if (use_stagefright_) {
    return StagefrightMetadataRetriever::extractMetadata(keyCode);
  }
  if (!is_metadata_extracted_) {
    if (!init_) {
      RETRIEVER_LOGW("Not initialized yet.");
      res = false;
    }
    char tmp[512];
    if (context_->duration > 0) {
      // Converts to a string in milliseconds.
      sprintf(tmp, "%lld", context_->duration * 1000 / AV_TIME_BASE);
      metadata_.add(android::METADATA_KEY_DURATION, String8(tmp));
    }
    if (!res) {
      RETRIEVER_LOGE("Failed to retrieve metadata from ffmpeg.");
    }
    is_metadata_extracted_ = true;
  }
  ssize_t index = metadata_.indexOfKey(keyCode);

  if (index < 0) {
    return NULL;
  }

  return strdup(metadata_.valueAt(index).string());
}


VideoFrame* MRVLFFMetadataRetriever::getFrameAtTime(int64_t timeUs, int option) {
  VideoFrame* frame = NULL;

  RETRIEVER_LOGD("MetadataRetriever getFrameAtTime %lld ,context_:0x%p", timeUs, context_);
  AutoMutex lock(lock_);

  if(NULL == context_){
    RETRIEVER_LOGE("context_ is NULL. return NULL");
    return NULL;
  }

  if (!init_) {
    RETRIEVER_LOGE("Not initialized yet.");
    return NULL;
  }
  int32_t video_index = -1;
  for (size_t i = 0; i < context_->nb_streams; ++i) {
    if (context_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_index = i;
      break;
    }
  }
  if (video_index == -1) {
    RETRIEVER_LOGE("Failed to find video stream.");
    return NULL;
  }

  AVCodecContext* codec_context = context_->streams[video_index]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
  if (codec == NULL) {
    RETRIEVER_LOGE("Failed to find matching decoder for codec id (%x)", codec_context->codec_id);
    return NULL;
  }

  int res = avcodec_open2(codec_context, codec, NULL);
  if (res < 0) {
    RETRIEVER_LOGE("avcodec_open() failed %d", res);
    return NULL;
  }

  AVPacket* packet = new AVPacket();
  AVFrame* decoded_frame = avcodec_alloc_frame();
  int32_t flags = 0;
  flags |= AVSEEK_FLAG_BACKWARD;

  int64_t duration = context_->duration;
  if((duration == int64_t(0x8000000000000000LL)) && !context_->bit_rate) {
    return NULL;
  }

  int64_t file_start_time = context_->start_time;
  int64_t seek_target = (duration >> 1) + file_start_time;

  seek_target= av_rescale_q(seek_target, AV_TIME_BASE_Q,
      context_->streams[video_index]->time_base);

  RETRIEVER_LOGD("Duration is %lld Going to seek to %lld", duration, seek_target);

  av_seek_frame(context_, video_index, seek_target, flags);

  bool got_video_pkt = false;
  bool decode_success = false;
  int attempt = 0;
  const int kMaxDecodeAttempts = 20;
  int got_picture = 0;
  while (!got_picture && attempt < kMaxDecodeAttempts) {
    while (av_read_frame(context_, packet) >= 0) {
      if ((packet->stream_index == video_index) && (packet->flags & AV_PKT_FLAG_KEY)) {
        got_video_pkt = true;
        RETRIEVER_LOGD("Found one key frame pts = %lld", packet->pts);
        break;
      }
      av_free_packet(packet);
    }
    if (false == got_video_pkt) {
      RETRIEVER_LOGE("Failed to find video packet.");
      return NULL;
    }
    avcodec_get_frame_defaults(decoded_frame);

    RETRIEVER_LOGD("Decode one frame, attempt = %d",attempt);
    int bytes_decoded = avcodec_decode_video2(
        codec_context, decoded_frame, &got_picture, packet);
    attempt++;
    av_free_packet(packet);
  }

  if (got_picture != 0) {
    RETRIEVER_LOGD("Decode success");
    decode_success = true;
  } else {
    RETRIEVER_LOGE("Decode failed");
  }

  delete packet;
  if (!decode_success) {
    av_free(decoded_frame);
    avcodec_close(codec_context);
    RETRIEVER_LOGE("!decode_success, return");
    return NULL;
  }

  if (codec_context->pix_fmt != PIX_FMT_YUV420P && codec_context->pix_fmt != PIX_FMT_YUVJ420P) {
    RETRIEVER_LOGE("Unsupported pixel format %d", codec_context->pix_fmt);
    av_free(decoded_frame);
    avcodec_close(codec_context);
    return NULL;
  }

  if ((decoded_frame->linesize[0] != decoded_frame->linesize[1] * 2)
      || (decoded_frame->linesize[0] != decoded_frame->linesize[2] * 2)) {
    RETRIEVER_LOGE("Unexpected linesize in AVFrame Y:%d U:%d V:%d",
         decoded_frame->linesize[0], decoded_frame->linesize[1], decoded_frame->linesize[2]);
    return NULL;
  }

  const int width = codec_context->width;
  const int height = codec_context->height;
  const int line_stride = decoded_frame->linesize[0];
  const int frame_size = height * line_stride;
  uint8_t* data = (uint8_t*)malloc(frame_size * 2);
  if (data == NULL) {
    RETRIEVER_LOGE("Not enough memory for allocating memory for the color conversion.");
    return NULL;
  }
  memcpy(data, decoded_frame->data[0], frame_size);
  memcpy(data + frame_size, decoded_frame->data[1], frame_size / 4);
  memcpy(data + frame_size + frame_size / 4, decoded_frame->data[2], frame_size / 4);

  frame = new VideoFrame();
  if (frame == NULL) {
    RETRIEVER_LOGE("Not enough memory for allocating memory for VideoFrame.");
    free(data);
    data = NULL;
    return NULL;
  }
  frame->mWidth = frame->mDisplayWidth = width;
  frame->mHeight = frame->mDisplayHeight = height;
  frame->mSize = frame_size * 2;
  frame->mRotationAngle = 0;
  frame->mData = new uint8_t[frame->mSize];

  android::ColorConverter converter(
      OMX_COLOR_FormatYUV420Planar, OMX_COLOR_Format16bitRGB565);
  CHECK(converter.isValid());

  status_t err = converter.convert(
      data,
      line_stride, height,
      0, 0, width - 1, height - 1,
      frame->mData,
      width,
      height,
      0, 0, width - 1, height - 1);
  free(data);
  data = NULL;

  av_free(decoded_frame);
  avcodec_close(codec_context);
  RETRIEVER_LOGD("Successfully decode the thumbnail");
  return frame;
}

}
