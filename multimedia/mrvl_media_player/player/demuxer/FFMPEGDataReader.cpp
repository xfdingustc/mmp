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

#include "FFMPEGDataReader.h"
#include "formatters/AVCCFormatter.h"
#include "formatters/RVFormatter.h"
#include "MediaPlayerOnlineDebug.h"

#undef  LOG_TAG
#define LOG_TAG "FFMPEGDataReader"

namespace mmp {
extern uint64_t OnlineDebugBitMask;

typedef struct {
  AVCodecID codec_id_;
  const char* mime_type_;
} AVCODEC_ID_MAP;

static AVCODEC_ID_MAP SUPPORTED_AVCODEC_ID_MAP[] = {
  { AV_CODEC_ID_WMV3,         MMP_MIMETYPE_VIDEO_WMV },
  { AV_CODEC_ID_MPEG4,        MMP_MIMETYPE_VIDEO_MPEG4 },
  { AV_CODEC_ID_MPEG1VIDEO,   MMP_MIMETYPE_VIDEO_MPEG2 },
  { AV_CODEC_ID_MPEG2VIDEO,   MMP_MIMETYPE_VIDEO_MPEG2 },
  { AV_CODEC_ID_H264,         MMP_MIMETYPE_VIDEO_AVC },
  { AV_CODEC_ID_VC1,          MMP_MIMETYPE_VIDEO_VC1 },
  { AV_CODEC_ID_MSMPEG4V3,    MMP_MIMETYPE_VIDEO_MSMPEG4 },
  { AV_CODEC_ID_H263,         MMP_MIMETYPE_VIDEO_H263 },
  { AV_CODEC_ID_H263I,        MMP_MIMETYPE_VIDEO_H263 },
  { AV_CODEC_ID_H263P,        MMP_MIMETYPE_VIDEO_H263 },
  { AV_CODEC_ID_VP8,          MMP_MIMETYPE_VIDEO_VPX },
  { AV_CODEC_ID_RV30,         MMP_MIMETYPE_VIDEO_RV },
  { AV_CODEC_ID_RV40,         MMP_MIMETYPE_VIDEO_RV },

  { AV_CODEC_ID_MP2,          MMP_MIMETYPE_AUDIO_MPEG_LAYER_II },
  { AV_CODEC_ID_MP1,          MMP_MIMETYPE_AUDIO_MPEG_LAYER_I },
  { AV_CODEC_ID_AC3,          MMP_MIMETYPE_AUDIO_AC3 },
  { AV_CODEC_ID_EAC3,         MMP_MIMETYPE_AUDIO_EAC3 },
  { AV_CODEC_ID_MP3,          MMP_MIMETYPE_AUDIO_MPEG },
  { AV_CODEC_ID_AAC,          MMP_MIMETYPE_AUDIO_AAC },
  { AV_CODEC_ID_PCM_MULAW,    MMP_MIMETYPE_AUDIO_G711_MLAW },
  { AV_CODEC_ID_PCM_ALAW,     MMP_MIMETYPE_AUDIO_G711_ALAW },
  { AV_CODEC_ID_PCM_S16LE,    MMP_MIMETYPE_AUDIO_RAW },
  { AV_CODEC_ID_PCM_S16BE,    MMP_MIMETYPE_AUDIO_RAW },
  { AV_CODEC_ID_PCM_U8,       MMP_MIMETYPE_AUDIO_RAW },
  { AV_CODEC_ID_PCM_BLURAY,   MMP_MIMETYPE_AUDIO_RAW },
  { AV_CODEC_ID_FLAC,         MMP_MIMETYPE_AUDIO_FLAC },
  { AV_CODEC_ID_WMAV1,        MMP_MIMETYPE_AUDIO_WMA },
  { AV_CODEC_ID_WMAV2,        MMP_MIMETYPE_AUDIO_WMA },
  { AV_CODEC_ID_WMAPRO,       MMP_MIMETYPE_AUDIO_WMA },
  { AV_CODEC_ID_WMALOSSLESS,  MMP_MIMETYPE_AUDIO_WMA },
  { AV_CODEC_ID_VORBIS,       MMP_MIMETYPE_AUDIO_VORBIS },
  { AV_CODEC_ID_AMR_NB,       MMP_MIMETYPE_AUDIO_AMR_NB },
  { AV_CODEC_ID_AMR_WB,       MMP_MIMETYPE_AUDIO_AMR_WB },
  { AV_CODEC_ID_COOK,         MMP_MIMETYPE_AUDIO_RA },
  { AV_CODEC_ID_DTS,          MMP_MIMETYPE_AUDIO_DTS },

  { AV_CODEC_ID_TEXT,         "text/plain" },
  { AV_CODEC_ID_SSA,          "text/x-ssa" },
  { AV_CODEC_ID_SUBRIP,       "text/plain" },
  { AV_CODEC_ID_MOV_TEXT,     "text/3gpp-tt" },
};


#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#endif

#ifndef INT64_MIN
#define INT64_MIN       (-0x7fffffffffffffffLL - 1)
#endif

#ifndef INT64_MAX
#define INT64_MAX  int64_t(9223372036854775807)
#endif


#define TIME_BASE_NUMER_1MHZ UINT64_C(1000000)
#define TIME_BASE_NUMER_1KHZ UINT64_C(1000)

static inline uint64_t GetTimeStampInUs(int64_t time_stamp, AVRational *pAVRational) {
  // If the pAVRational->num is too big, we have to use float type, otherwise it will overflow
  if (pAVRational->num > INT64_MAX / (int64_t)TIME_BASE_NUMER_1MHZ / 1000000) {
    return ((float)pAVRational->num / pAVRational->den) * TIME_BASE_NUMER_1MHZ * time_stamp;
  }

  if (INT64_MAX / TIME_BASE_NUMER_1MHZ >=
     (time_stamp < 0) ? time_stamp * -1 : time_stamp) {
   return (time_stamp * TIME_BASE_NUMER_1MHZ * pAVRational->num) /
       static_cast<int64_t>(pAVRational->den);
  } else {
   return (time_stamp * TIME_BASE_NUMER_1KHZ * pAVRational->num) /
       static_cast<int64_t>(pAVRational->den * UINT64_C(1000000));
  }
}

FFMPEGDemuxTask::FFMPEGDemuxTask(FFMPEGDataReader* demux)
  : demux_(demux){
}


mbool FFMPEGDemuxTask::ThreadLoop() {
  demux_->mainTaskEntry();
  return true;
}



FFMPEGDataReader::FFMPEGDataReader(MRVLDataReaderCbTarget* cb_target)
  : pMediaInfo_(NULL),
    frames_without_I_frames_(0),
    check_I_frames_(false),
    start_time_(0),
    duration_(0),
    current_video_(0),
    current_audio_(0),
    b_seek_by_bytes_(false),
    cb_target_(cb_target),
    mAVFormatContext_(NULL),
    mMRVLFormatter_(NULL),
    main_task_(NULL),
    dr_state_(TS_DR_IDLE),
    RawDataReader("ffmpeg-demux") {
  DATA_READER_RUNTIME("ENTER");

  for (uint32_t i = 0; i < MAX_SUPPORT_STREAMS; i++) {
    // PTS
    b_pts_reliable_[i] = true;
    last_pts_[i] = AV_NOPTS_VALUE;
    // DTS
    b_dts_reliable_[i] = true;
    last_dts_[i] = AV_NOPTS_VALUE;

    memset(&sub_streams_[i], 0, sizeof(FFStream));
  }

  b_first_key_frame_after_seek_ = true;

  av_register_all();
  avformat_network_init();

  b_pending_seek_ = false;
  video_pts_offset_ = 0;
  last_video_pts_ = kInvalidTimeStamp;
  video_pts_delta_ = 0;
  last_seen_pts_ = kInvalidTimeStamp;
  b_datareader_eos_ = false;
  b_getting_audio_pts_after_seek_ = false;
  audio_pts_after_seek_ = kInvalidTimeStamp;
  b_getting_video_pts_after_seek_ = false;
  video_pts_after_seek_ = kInvalidTimeStamp;

  mMsgQLock_ = kdThreadMutexCreate(KD_NULL);
  if (!mMsgQLock_) {
    DATA_READER_RUNTIME("Out of memory, can't create mMsgQLock_!");
  }

  DATA_READER_RUNTIME("EXIT");
}

FFMPEGDataReader::~FFMPEGDataReader() {
  DATA_READER_RUNTIME("ENTER");
  uint32_t i = 0;

  cb_target_ = NULL;

  if (main_task_) {
    ReaderCommand *rCmd = new ReaderCommand(TS_DR_EXIT);
    if (rCmd) {
      kdThreadMutexLock(mMsgQLock_);
      mMsgQ_.push_back(rCmd);
      kdThreadMutexUnlock(mMsgQLock_);

      /* Wait DR command done */
      rCmd->mCompleteSignal.waitSignal();

      delete rCmd;
    }

    main_task_->RequestExit();
    main_task_->Join();

    delete main_task_;
    main_task_ = NULL;
    DATA_READER_RUNTIME("now work thread is already shut down.");
  }

  if (mMRVLFormatter_) {
    delete mMRVLFormatter_;
    mMRVLFormatter_ = NULL;
  }

  CloseFFmpeg();

  mMsgQ_.clear();

  if (mMsgQLock_) {
    kdThreadMutexFree(mMsgQLock_);
  }

  DATA_READER_RUNTIME("EXIT");
}


MediaResult FFMPEGDataReader::CloseFFmpeg(){
  DATA_READER_RUNTIME("free ffmpeg");
  if (mAVFormatContext_) {
    avformat_close_input(&mAVFormatContext_);
    mAVFormatContext_ = NULL;
  }
  DATA_READER_RUNTIME("free ffmpeg over");
  return kSuccess;
}

bool FFMPEGDataReader::setPCMProperty(MmpCaps &caps, AVCodecID codec_id) {
  mint32 bits_per_sample = 0;
  mint32 data_sign = 0; // 0: signed, 1: unsigned
  mint32 data_endian = 0; // 0: big endian, 1: little endian
  mint32 pcm_mode = 0; // 0: PCMModeLinear, 1: PCMModeBluray

  switch (codec_id) {
    case AV_CODEC_ID_PCM_U8:
      bits_per_sample = 8;
      data_sign = 1; // unsigned
      break;
    case AV_CODEC_ID_PCM_S16LE:
      bits_per_sample = 16;
      data_endian = 1; // little endian
      break;
    case AV_CODEC_ID_PCM_S16BE:
      bits_per_sample = 16;
      break;
    case AV_CODEC_ID_PCM_S24LE:
      bits_per_sample = 24;
      data_endian = 1; // little endian
      break;
    case AV_CODEC_ID_PCM_S24BE:
      bits_per_sample = 24;
      break;
    case AV_CODEC_ID_PCM_BLURAY:
      data_endian = 1;  // big endian
      pcm_mode = 1; // PCMModeBluray
    default:
      bits_per_sample = 16;
  }

  caps.SetInt32("bits_per_sample", bits_per_sample);
  caps.SetInt32("data_sign", data_sign);
  caps.SetInt32("data_endian", data_endian);
  caps.SetInt32("pcm_mode", pcm_mode);

  return true;
}

bool FFMPEGDataReader::setWMAProperty(MmpCaps &caps, AVCodecContext *codec_ctx) {
  mint32 bits_per_sample = 0, codec_tag = 0, bit_rate = 0, block_align = 0;

  if (!codec_ctx) {
    DATA_READER_RUNTIME("No codec_ctx found!");
    return false;
  }

  caps.SetInt32("bits_per_sample", codec_ctx->bits_per_coded_sample);
  caps.SetInt32("bit_rate", codec_ctx->bit_rate);
  caps.SetInt32("block_align", codec_ctx->block_align);
  caps.SetInt32("codec_tag", codec_ctx->codec_tag);

  return true;
}

bool FFMPEGDataReader::setRAProperty(MmpCaps &caps, AVStream *av_stream) {
  struct RMStream {
    AVPacket pkt;      ///< place to store merged video frame / reordered audio data
    int videobufsize;  ///< current assembled frame size
    int videobufpos;   ///< position for the next slice in the video buffer
    int curpic_num;    ///< picture number of current frame
    int cur_slice, slices;
    int64_t pktpos;    ///< first slice position in file
    /// Audio descrambling matrix parameters
    int64_t audiotimestamp; ///< Audio packet timestamp
    int sub_packet_cnt; // Subpacket counter, used while reading
    int sub_packet_size, sub_packet_h, coded_framesize; ///< Descrambling parameters from container
    int audio_framesize; /// Audio frame size from container
    int sub_packet_lengths[16]; /// Length of each subpacket
    int32_t deint_id;  ///< deinterleaver used in audio stream
    int flavor;
  };

  if (!av_stream || !av_stream->priv_data ) {
    DATA_READER_RUNTIME("No config data found!");
    return false;
  }

  RMStream *raStream = reinterpret_cast<RMStream*>(av_stream->priv_data);
  AVCodecContext *codec_ctx = av_stream->codec;

  caps.SetInt32("bits_per_sample", codec_ctx->bits_per_coded_sample);
  caps.SetInt32("sub_packet_size", raStream->sub_packet_size);
  caps.SetInt32("flavor_index", raStream->flavor);
  caps.SetInt32("coded_framesize", raStream->coded_framesize);

  return true;
}

MediaResult FFMPEGDataReader::ProbeStream(MEDIA_FILE_INFO *media_info) {
  int32_t err;
  uint32_t i;

  err = avformat_find_stream_info(mAVFormatContext_, NULL);
  if (err < 0) {
    DATA_READER_RUNTIME("parse stream info failed");
    if (mAVFormatContext_) {
      avformat_close_input(&mAVFormatContext_);
      mAVFormatContext_ = NULL;
    }
    return kUnexpectedError;
  }

  av_dump_format(mAVFormatContext_, 0, 0, 0);

  //to do
  media_info->file_size = 0x0;
  media_info->bit_rate = mAVFormatContext_->bit_rate;

  if (strcmp(mAVFormatContext_->iformat->name, "flac") == 0) {
     b_seek_by_bytes_ = true;
    DATA_READER_RUNTIME("Stream contains flac, should seek by  bytes!");
  }

  start_time_ = mAVFormatContext_->start_time;
  media_info->start_time = start_time_;

  if ((mAVFormatContext_->start_time == (int64_t)AV_NOPTS_VALUE) ||
      (mAVFormatContext_->start_time < 0)) {
    DATA_READER_RUNTIME("start_time got from mAVFormatContext_->start_time is 0.");
    start_time_ = 0;
    media_info->start_time = 0;
  }

  duration_ = mAVFormatContext_->duration;
  if (AV_NOPTS_VALUE == duration_) {
    DATA_READER_RUNTIME("Duration got from mAVFormatContext_->duration is 0.");
    duration_ = 0;
  }
  media_info->duration = duration_ * (1000000 / AV_TIME_BASE);

  DATA_READER_RUNTIME("start time: "TIME_FORMAT"(%lld us), duration: "TIME_FORMAT"(%lld us)",
      TIME_ARGS(media_info->start_time), media_info->start_time,
      TIME_ARGS(media_info->duration), media_info->duration);

  for (i = 0; i < mAVFormatContext_->nb_streams; i++) {
    AVStream *av_stream = mAVFormatContext_->streams[i];
    AVCodecContext *enc = av_stream->codec;
    int j;

    switch(enc->codec_type) {
      case AVMEDIA_TYPE_SUBTITLE: {
        DATA_READER_RUNTIME("in mux codec id is 0x%x ,stream idx is %d", enc->codec_id, i);
        media_info->total_subtitle_counts++;
        const mchar* mime_type = getMimeType(enc->codec_id);
        if (mime_type && media_info->supported_subtitle_counts < MAX_SUPPORT_STREAMS) {
          MV_STREAM_INFO *pStreamInfo =
              &media_info->subtitle_stream_info[media_info->supported_subtitle_counts];
          pStreamInfo->codec_id = enc->codec_id;
          pStreamInfo->stream_index = i;

          pStreamInfo->avstream = av_stream;

          FFStream *sub_stream = &sub_streams_[i];

          // Create Mmp Pads
          MmpPad* subtitle_pad = new MmpPad("subtitle", MmpPad::MMP_PAD_SRC,
                                            MmpPad::MMP_PAD_MODE_PUSH);
          M_ASSERT_FATAL(subtitle_pad, kUnexpectedError);

          MmpCaps &sub_pad_caps = subtitle_pad->GetCaps();

          sub_pad_caps.SetInt32("codec_id", enc->codec_id);
          sub_pad_caps.SetString("mime_type", mime_type);

          // Get subtitle language, description, etc.
          AVDictionaryEntry *lang;
          if (((lang = av_dict_get(av_stream->metadata, "language", NULL, 0)) && lang)
              || ((lang = av_dict_get(av_stream->metadata, "lang", NULL, 0)) && lang)
              || ((lang = av_dict_get(av_stream->metadata, "tlan", NULL, 0)) && lang)) {
            sub_pad_caps.SetString("language", lang->value);
          }

          AVDictionaryEntry *desp;
          if (((desp = av_dict_get(av_stream->metadata, "description", NULL, 0)) && desp)
              || ((desp = av_dict_get(av_stream->metadata, "comment", NULL, 0)) && desp)
              || ((desp = av_dict_get(av_stream->metadata, "title", NULL, 0)) && desp)) {
            sub_pad_caps.SetString("description", desp->value);
          }

          media_info->supported_subtitle_counts++;

          AddPad(subtitle_pad);
          sub_stream->pad = subtitle_pad;

        } else {
          DATA_READER_RUNTIME("!!!Warning: subtitle stream is above maximum default "
              "stream numbers: %d, or unknown format: 0x%x",
              i, enc->codec_id);
        }
        break;
      }

      case AVMEDIA_TYPE_AUDIO: {
        media_info->total_audio_counts++;
        const mchar* mime_type = getMimeType(enc->codec_id);

        if (mime_type && media_info->supported_audio_counts < MAX_SUPPORT_STREAMS
            && (mAVFormatContext_->streams[i]->codec_info_nb_frames >= 0)) {
          MV_STREAM_INFO *pStreamInfo;

          pStreamInfo = &media_info->audio_stream_info[media_info->supported_audio_counts];

          pStreamInfo->avstream = av_stream;

          pStreamInfo->codec_id = enc->codec_id;
          pStreamInfo->stream_index = i;

          FFStream *sub_stream = &sub_streams_[i];
          // Create Mmp Pads
          MmpPad* audio_pad = new MmpPad("audio", MmpPad::MMP_PAD_SRC,
                                            MmpPad::MMP_PAD_MODE_PUSH);
          M_ASSERT_FATAL(audio_pad, kUnexpectedError);

          MmpCaps &audio_pad_caps = audio_pad->GetCaps();

          audio_pad_caps.SetInt32("codec_id", enc->codec_id);
          audio_pad_caps.SetString("mime_type", mime_type);
          audio_pad_caps.SetInt32("sample_rate", enc->sample_rate);
          audio_pad_caps.SetInt32("channels", enc->channels);
          audio_pad_caps.SetData("codec-specific-data", enc->extradata, enc->extradata_size);
          if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_RAW)) {
            setPCMProperty(audio_pad_caps, enc->codec_id);
          } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_WMA)) {
            setWMAProperty(audio_pad_caps, enc);
          } else if (!strcmp(mime_type, MMP_MIMETYPE_AUDIO_RA)) {
            setRAProperty(audio_pad_caps, av_stream);
          }

          // Get subtitle language, description, etc.
          AVDictionaryEntry *lang;
          if (((lang = av_dict_get(av_stream->metadata, "language", NULL, 0)) && lang)
              || ((lang = av_dict_get(av_stream->metadata, "lang", NULL, 0)) && lang)
              || ((lang = av_dict_get(av_stream->metadata, "tlan", NULL, 0)) && lang)) {
            audio_pad_caps.SetString("language", lang->value);
          }

          AVDictionaryEntry *desp;
          if (((desp = av_dict_get(av_stream->metadata, "description", NULL, 0)) && desp)
              || ((desp = av_dict_get(av_stream->metadata, "comment", NULL, 0)) && desp)
              || ((desp = av_dict_get(av_stream->metadata, "title", NULL, 0)) && desp)) {
            audio_pad_caps.SetString("description", desp->value);
          }

          AddPad(audio_pad);
          sub_stream->pad = audio_pad;

          if(current_audio_ == -1) {
            current_audio_ = i;
          }

          media_info->supported_audio_counts++;
        } else {
          DATA_READER_RUNTIME("!!!Warning: audio stream is above maximum "
              "default stream numbers:%d, or unknown format: 0x%x",
              i, enc->codec_id);
        }
        break;
      }

      case AVMEDIA_TYPE_VIDEO: {
        media_info->total_video_counts++;
        const mchar* mime_type = getMimeType(enc->codec_id);
        if (mime_type && media_info->supported_video_counts < MAX_SUPPORT_STREAMS) {
          MV_STREAM_INFO *pStreamInfo;

          /* Current video stream index: used by trick play */
          pStreamInfo = &media_info->video_stream_info[0];

          pStreamInfo->avstream = av_stream;

          if (current_video_ == -1) {
            current_video_ = i;
          }

          pStreamInfo->codec_id = enc->codec_id;
          pStreamInfo->stream_index = i;

          FFStream *sub_stream = &sub_streams_[i];
          // Create Mmp Pads
          MmpPad* video_pad = new MmpPad("video", MmpPad::MMP_PAD_SRC,
                                            MmpPad::MMP_PAD_MODE_PUSH);
          M_ASSERT_FATAL(video_pad, kUnexpectedError);

          // NOTE: this framerate value is just a guess from FFmpeg. Decoder
          // should get and use the real framerate from decoding.
          AVRational *rationalFramerate = NULL;
          uint64_t framerate = 0;
          if (av_stream->avg_frame_rate.num > 0 && av_stream->avg_frame_rate.den > 0) {
            rationalFramerate = &(av_stream->avg_frame_rate);
          } else if (av_stream->r_frame_rate.num > 0 && av_stream->r_frame_rate.den > 0) {
            rationalFramerate = &(av_stream->r_frame_rate);
          }
          if (rationalFramerate != NULL) {
            framerate = rationalFramerate->num;
            framerate <<= 16;  // convert to Q16 value.
            framerate /= rationalFramerate->den;
          }

          MmpCaps &video_pad_caps = video_pad->GetCaps();

          video_pad_caps.SetString("mime_type", mime_type);
          video_pad_caps.SetInt32("width", enc->width);
          video_pad_caps.SetInt32("height", enc->height);
          video_pad_caps.SetInt32("profile", enc->profile);
          video_pad_caps.SetInt32("level", enc->level);
          video_pad_caps.SetInt32("frame_rate", framerate);
          if (AV_CODEC_ID_RV30 == enc->codec_id) {
            video_pad_caps.SetInt32("rv_type", 8);
          } else if (AV_CODEC_ID_RV40 == enc->codec_id) {
            video_pad_caps.SetInt32("rv_type", 9);
          }
          video_pad_caps.SetData("codec-specific-data", enc->extradata, enc->extradata_size);

          AddPad(video_pad);
          sub_stream->pad = video_pad;

          if (AV_CODEC_ID_H264 == enc->codec_id) {
            mMRVLFormatter_ = new AVCCFormatter(enc);
          } else if ((AV_CODEC_ID_RV30 == enc->codec_id) ||
              (AV_CODEC_ID_RV40 == enc->codec_id)) {
            mMRVLFormatter_ = new RVFormatter(enc);
          }

          /* Run to next video stream */
          media_info->supported_video_counts = 1;
        } else {
          DATA_READER_RUNTIME("!!!Warning: video stream is above maximum "
              "default stream numbers: %d, or unknown format: 0x%x",
              i, enc->codec_id);
        }
        break;
      }

      default:
        media_info->total_data_counts++;
        DATA_READER_RUNTIME("!!!Warning: unknown stream type:%d\n", enc->codec_type);
        break;
    }
  }

  /* The duration calculated by FFmpeg may be wrong. */
  CheckDuration(media_info);

  pMediaInfo_ = media_info;
  return kSuccess;
}

MediaResult FFMPEGDataReader::CheckDuration(MEDIA_FILE_INFO *pMediaInfo)
{
  /* Timestamp in TS can be discontinuous, so the duration is NOT always trustable.
     Check the average bitrate to detect wrong duration. */
  if ((!strncmp(mAVFormatContext_->iformat->name, "mpegts", strlen("mpegts"))) &&
      (pMediaInfo->total_video_counts > 0)) {
    /* These two empirical values are suitable for SD and HD (720p/1080p).
       If one day MMP needs to support 4k, then we need to define extra values
       for it. */
    const int kTsMinRateSupposed = 500 * 1000;
    const int kTsMaxRateSupposed = 55 * 1000 * 1000;
    if ((mAVFormatContext_->bit_rate < kTsMinRateSupposed) ||
        (mAVFormatContext_->bit_rate > kTsMaxRateSupposed)) {
      DATA_READER_RUNTIME("!!!Warning: Average bitrate %d kb/s is out of normal range."
                          " (%d kb/s ~ %d kb/s)\n",
                          mAVFormatContext_->bit_rate / 1000,
                          kTsMinRateSupposed / 1000, kTsMaxRateSupposed / 1000);
      duration_ = 0;
      pMediaInfo->duration = 0;
    }
  }

  return kSuccess;
}

MediaResult FFMPEGDataReader::Probe(const char *url, MEDIA_FILE_INFO *pMediaInfo) {
  int i = 0;
  int64_t ti = 0;
  int err = 0;

  if (mAVFormatContext_) {
    avformat_close_input(&mAVFormatContext_);
    mAVFormatContext_ = NULL;
  }

  err = avformat_open_input(&mAVFormatContext_, url, NULL, NULL);
  if (err < 0) {
    DATA_READER_RUNTIME("%s line %d: URL(%s) open failed!", __FUNCTION__, __LINE__, url);
    return kUnexpectedError;
  }

  current_audio_ = -1;
  current_video_ = -1;
  current_subtitle_ = -1;
  b_seek_by_bytes_ = false;

  return ProbeStream(pMediaInfo);
}


//For ffmpeg 0.10.2, need todo
MediaResult FFMPEGDataReader::Probe(const char *url, void *pb,
    void *fmt, MEDIA_FILE_INFO *pMediaInfo) {
  if (!pMediaInfo) {
    DATA_READER_ERROR("bad pointer");
    return kBadPointer;
  }

  if (!pb || !fmt) {
    DATA_READER_RUNTIME("pb %p, fmt %p",pb, fmt);
    return kUnexpectedError;
  }

  if(!mAVFormatContext_) {
    mAVFormatContext_ = avformat_alloc_context();
    if(!mAVFormatContext_) {
      DATA_READER_RUNTIME("avformat_alloc_context failed");
      return kUnexpectedError;
    }
  }
  mAVFormatContext_->pb = (AVIOContext*)pb;
  //mAVFormatContext_->pb->seekable = true;

  // If format is not given, we will probe the format first
  if (avformat_open_input(&mAVFormatContext_, "", (AVInputFormat*)fmt, NULL) < 0) {
    // Don't free anything here, resource will be recycled when CorePlayer is destroyed.
    // Free pb here will crash because it is a member of StagefrightHLSWrapper,
    // it is not from malloc.
    DATA_READER_RUNTIME("avformat_open_input failed");
    return kUnexpectedError;
  }
  current_audio_ = -1;
  current_video_ = -1;
  current_subtitle_ = -1;
  b_seek_by_bytes_ = false;

  return ProbeStream(pMediaInfo);
}

MediaResult FFMPEGDataReader::Init() {
  main_task_ = new FFMPEGDemuxTask(this);
  if (main_task_) {
    main_task_->Run();
  }
  return kSuccess;
}

MediaResult FFMPEGDataReader::Start(double speed) {
  MediaResult ret = kSuccess;
  DATA_READER_RUNTIME("DR start");
  ReaderCommand *rCmd = new ReaderCommand(TS_DR_PLAY);
  if (rCmd) {
    kdThreadMutexLock(mMsgQLock_);
    mMsgQ_.push_back(rCmd);
    kdThreadMutexUnlock(mMsgQLock_);

    /* Wait DR command done */
    rCmd->mCompleteSignal.waitSignal();
    DATA_READER_RUNTIME("FFMPEGDataReader starts reading data");

    delete rCmd;
  } else {
    DATA_READER_RUNTIME("Out of memory!");
    ret = kUnexpectedError;
  }
  DATA_READER_RUNTIME("DR start done.");
  return ret;
}

MediaResult FFMPEGDataReader::Stop() {
  MediaResult ret = kSuccess;
  DATA_READER_RUNTIME("DR stop");
  ReaderCommand *rCmd = new ReaderCommand(TS_DR_STOP);
  if (rCmd) {
    kdThreadMutexLock(mMsgQLock_);
    mMsgQ_.push_back(rCmd);
    kdThreadMutexUnlock(mMsgQLock_);

    /* Wait DR command done */
    rCmd->mCompleteSignal.waitSignal();
    DATA_READER_RUNTIME("DR stop done.");

    delete rCmd;
  } else {
    DATA_READER_RUNTIME("Out of memory!");
    ret = kUnexpectedError;
  }
  return ret;
}

MediaResult FFMPEGDataReader::Seek(uint64_t seek_pos_us) {
  MediaResult ret = kSuccess;
  DATA_READER_RUNTIME("DR seek to %lld us.", seek_pos_us);
  ReaderCommand *rCmd = new ReaderCommand(TS_DR_SEEK, seek_pos_us);
  if (rCmd) {
    kdThreadMutexLock(mMsgQLock_);
    mMsgQ_.push_back(rCmd);
    kdThreadMutexUnlock(mMsgQLock_);

    // Wait DR command done
    rCmd->mCompleteSignal.waitSignal();
    DATA_READER_RUNTIME("DR seek done.");

    delete rCmd;
  } else {
    DATA_READER_RUNTIME("Out of memory!");
    ret = kUnexpectedError;
  }

  // When seek, reset last_pts_[i]/last_dts_[i].
  for (uint32_t i = 0; i < MAX_SUPPORT_STREAMS; i++) {
    last_pts_[i] = AV_NOPTS_VALUE;
    last_dts_[i] = AV_NOPTS_VALUE;
  }

  b_pending_seek_ = true;
  video_pts_offset_ = 0;
  last_video_pts_ = kInvalidTimeStamp;
  last_seen_pts_ = kInvalidTimeStamp;
  b_datareader_eos_ = false;
  if (current_audio_ >= 0) {
    b_getting_audio_pts_after_seek_ = true;
    audio_pts_after_seek_ = kInvalidTimeStamp;
  }
  if (current_video_ >= 0) {
    b_getting_video_pts_after_seek_ = true;
    video_pts_after_seek_ = kInvalidTimeStamp;
  }

  return ret;
}

MediaResult FFMPEGDataReader::FlushDataReader() {
  if (mAVFormatContext_) {
    av_read_frame_flush(mAVFormatContext_);
    avio_flush(mAVFormatContext_->pb);
    avio_seek(mAVFormatContext_->pb, 0, SEEK_SET);
    DATA_READER_RUNTIME("%s() line %d, flush FFMPEG done.", __FUNCTION__, __LINE__);
  }

  // When seek, reset last_pts_[i]/last_dts_[i].
  for (uint32_t i = 0; i < MAX_SUPPORT_STREAMS; i++) {
    last_pts_[i] = AV_NOPTS_VALUE;
    last_dts_[i] = AV_NOPTS_VALUE;
  }

  b_pending_seek_ = true;
  video_pts_offset_ = 0;
  last_video_pts_ = kInvalidTimeStamp;
  last_seen_pts_ = kInvalidTimeStamp;
  b_datareader_eos_ = false;
  if (current_audio_ >= 0) {
    b_getting_audio_pts_after_seek_ = true;
    audio_pts_after_seek_ = kInvalidTimeStamp;
  }
  if (current_video_ >= 0) {
    b_getting_video_pts_after_seek_ = true;
    video_pts_after_seek_ = kInvalidTimeStamp;
  }

  return kSuccess;
}

MediaResult FFMPEGDataReader::SetCurrentStream(
  int32_t video_idx, int32_t audio_idx) {
  current_video_ = video_idx;
  current_audio_ = audio_idx;
  DATA_READER_RUNTIME("current_video_ = %d, current_audio_ = %d",
      current_video_, current_audio_);
  updateVideoFrameRate(current_video_);
  return kSuccess;
}

bool FFMPEGDataReader::updateVideoFrameRate(int32_t stream_idx) {
  if(stream_idx < 0) {
      return false;
  }

  AVStream *avstream = mAVFormatContext_->streams[stream_idx];;
  if(avstream) {
      video_pts_delta_ =
          (int64_t)1000000 * avstream->r_frame_rate.den / avstream->r_frame_rate.num;
      DATA_READER_RUNTIME("Got video PTS delta %lld us, frame rate %d/%d, video_pts_delta_ = "
          TIME_FORMAT"(%lld us).",
          video_pts_delta_, avstream->r_frame_rate.num, avstream->r_frame_rate.den,
          TIME_ARGS(video_pts_delta_), video_pts_delta_);
  }

  return true;
}

const mchar* FFMPEGDataReader::getMimeType(AVCodecID codec_id) {
  muint32 i = 0;

  for(i = 0; i < (sizeof(SUPPORTED_AVCODEC_ID_MAP) / sizeof(AVCODEC_ID_MAP)); i++) {
    AVCODEC_ID_MAP* codec_item = &SUPPORTED_AVCODEC_ID_MAP[i];
    if (codec_item->codec_id_ == codec_id) {
      return codec_item->mime_type_;
    }
  }

  return NULL;
}

MediaResult FFMPEGDataReader::stream_seek(int64_t timestamp, int32_t flags) {
  DATA_READER_RUNTIME("ENTER");
  MediaResult hr = kSuccess;

  if (b_seek_by_bytes_) {
    DATA_READER_RUNTIME("seek by bytes!");
    hr = stream_seek_by_byte(timestamp);
  } else {
    DATA_READER_RUNTIME("seek by frame!");
    hr = stream_seek_by_frame(timestamp, flags);
    if (hr != kSuccess) {
      b_seek_by_bytes_ = !!(mAVFormatContext_->iformat->flags & AVFMT_TS_DISCONT);
      DATA_READER_ERROR("seek by frame failed, try to seek by bytes, "
          "b_seek_by_bytes_ = %d", b_seek_by_bytes_);
      if (b_seek_by_bytes_) {
        hr = stream_seek_by_byte(timestamp);
      }
    }
  }
  b_first_key_frame_after_seek_ = true;

  DATA_READER_RUNTIME("EXIT");
  return hr;
}

MediaResult FFMPEGDataReader::stream_seek_by_byte(int64_t timestamp) {
  DATA_READER_RUNTIME("ENTER");
  int64_t seek_min = 0, seek_max = 0, curr_pos = 0, seek_target = 0;
  int seek_flags = 0;
  int hr = 0;

  // seek data by bytes
  // seek_target unit:second, relative postion
  seek_target = (timestamp - start_time_) / AV_TIME_BASE;
  curr_pos = (curr_pos - start_time_) / AV_TIME_BASE;
  if ((current_video_ >= 0) || (current_audio_ >= 0)) {
    if (mAVFormatContext_->bit_rate) {
      seek_target *= mAVFormatContext_->bit_rate / 8.0;
      curr_pos *= mAVFormatContext_->bit_rate / 8.0;
    } else {
      seek_target *= 180000.0;
      curr_pos *= 180000.0;
    }

    seek_flags &= ~AVSEEK_FLAG_BYTE;
    seek_flags |= AVSEEK_FLAG_BYTE;

    seek_min = (seek_target>curr_pos) ? curr_pos + 2: INT64_MIN;
    seek_max = (seek_target<curr_pos) ? curr_pos - 2: INT64_MAX;

    if(current_video_ >= 0) {
      hr = avformat_seek_file(mAVFormatContext_, current_video_,
          seek_min, seek_target, seek_max, seek_flags);
    } else if (current_audio_ >= 0) {
      hr = avformat_seek_file(mAVFormatContext_, current_audio_,
          seek_min, seek_target, seek_max, seek_flags);
    } else {
      DATA_READER_ERROR("Bad stream, no audio/video, can't seek.");
      return kUnexpectedError;
    }

    if (hr < 0) {
      DATA_READER_ERROR("Error in seek, err code = %d", hr);
      return kUnexpectedError;
    }
    DATA_READER_RUNTIME("EXIT");
    return kSuccess;
  } else {
    DATA_READER_ERROR("Bad stream, no audio/video, can't seek.");
    return kUnexpectedError;
  }
  DATA_READER_RUNTIME("EXIT");
  return kSuccess;
}


MediaResult FFMPEGDataReader::stream_seek_by_frame(int64_t timestamp, int32_t flags) {
  DATA_READER_RUNTIME("ENTER");
  int64_t seek_target = 0;
  int hr = 0;

  seek_target = timestamp;
  //seek data by time
  if (current_video_ >= 0) {
    seek_target = av_rescale_q(seek_target, AV_TIME_BASE_Q,
        mAVFormatContext_->streams[current_video_]->time_base);
    DATA_READER_RUNTIME("current_video = %d, flag = 0x%x, seek_target = %lld, "
        "timestamp = %lld us, time_base.den = %d, time_base.num = %d",
        current_video_, flags, seek_target, timestamp,
        mAVFormatContext_->streams[current_video_]->time_base.den,
        mAVFormatContext_->streams[current_video_]->time_base.num);
    if ((hr = av_seek_frame(mAVFormatContext_, current_video_, seek_target, flags)) < 0) {
      flags |= AVSEEK_FLAG_BACKWARD;
      DATA_READER_ERROR("Error in seek, err code = %d, "
          "try seek backward with flags = 0x%x", hr, flags);
      hr = av_seek_frame(mAVFormatContext_, current_video_, seek_target, flags);
      if (hr < 0) {
        DATA_READER_ERROR("seek backward failed, err code = %d", hr);
        return kUnexpectedError;
      }
    }

    // Check if current dts is valid
    TimeStamp current_pts = mAVFormatContext_->streams[current_video_]->cur_dts;
    if (current_pts != (int64_t)AV_NOPTS_VALUE
        && current_pts > mAVFormatContext_->duration + mAVFormatContext_->start_time) {
      DATA_READER_ERROR("After seek, current dts is valid, seek failed!");
      return kUnexpectedError;
    }

  } else if (current_audio_ >= 0) {
    seek_target = av_rescale_q(seek_target, AV_TIME_BASE_Q,
        mAVFormatContext_->streams[current_audio_]->time_base);
    if ((hr = av_seek_frame(mAVFormatContext_, current_audio_, seek_target, flags)) < 0) {
      DATA_READER_ERROR("Error in seek, err code = %d", hr);
      return kUnexpectedError;
    }
  } else {
    DATA_READER_ERROR("Bad stream, no audio/video, can't seek.");
    return kUnexpectedError;
  }
  DATA_READER_RUNTIME("EXIT");
  return kSuccess;
}

bool FFMPEGDataReader::calculatePTS(MmpBuffer *pAVPkt, AVPacket *pkt) {
  if (!pkt) {
    return true;
  }

  if ((pkt->pts != AV_NOPTS_VALUE) && (pkt->pts < 0)) {
    DATA_READER_ERROR("Bad pts, pts should not be negative!");
    pkt->pts = AV_NOPTS_VALUE;
  }

  AVStream *current_stream = mAVFormatContext_->streams[pkt->stream_index];

  // Some stream has a constant PTS. For such PTS, consider it as invalid PTS.
  if (b_pts_reliable_[pkt->stream_index] &&
      (pkt->pts != AV_NOPTS_VALUE) &&
      (last_pts_[pkt->stream_index] != AV_NOPTS_VALUE) &&
      (last_pts_[pkt->stream_index] == pkt->pts)) {
    b_pts_reliable_[pkt->stream_index] = false;
    last_pts_[pkt->stream_index] = AV_NOPTS_VALUE;
  }
  if (b_pts_reliable_[pkt->stream_index]) {
    last_pts_[pkt->stream_index] = pkt->pts;
  } else {
    pkt->pts = AV_NOPTS_VALUE;
  }

  // Some stream has a constant DTS. For such DTS, consider it as invalid DTS.
  if (b_dts_reliable_[pkt->stream_index] &&
      (pkt->dts != AV_NOPTS_VALUE) &&
      (last_dts_[pkt->stream_index] != AV_NOPTS_VALUE) &&
      (last_dts_[pkt->stream_index] == pkt->dts)) {
    b_dts_reliable_[pkt->stream_index] = false;
    last_dts_[pkt->stream_index] = AV_NOPTS_VALUE;
  }
  if (b_dts_reliable_[pkt->stream_index]) {
    last_dts_[pkt->stream_index] = pkt->dts;
  } else {
    pkt->dts = AV_NOPTS_VALUE;
  }

  // If PTS is AV_NOPTS_VALUE, we have to resume pts from dts for I frames,
  // in order to get better A/V sync performance.
  // For PTS & DTS invalid case, OMX will resume PTS for the first frame.
  if (pkt->pts != (int64_t)AV_NOPTS_VALUE && b_pts_reliable_[pkt->stream_index]) {
    pAVPkt->pts = GetTimeStampInUs(pkt->pts, &(current_stream->time_base));
  }

  // If it is the first key video frame after seek and it has invalid pts,
  // we have to use dts to restore the pts. Otherwise, it may cause av sync issue
  if (b_first_key_frame_after_seek_ && pkt->stream_index == current_video_) {
    if (pkt->flags & AV_PKT_FLAG_KEY) {
      if (pkt->pts == AV_NOPTS_VALUE
          && pkt->dts != AV_NOPTS_VALUE
          && b_pts_reliable_[pkt->stream_index]) {
        pAVPkt->pts = GetTimeStampInUs(pkt->dts, &(current_stream->time_base));
      }
      b_first_key_frame_after_seek_ = false;
    }
  }

  // For Subtitle render
  if (pkt->convergence_duration != (int64_t)AV_NOPTS_VALUE &&
      pkt->convergence_duration != 0) {
    pAVPkt->end_pts = GetTimeStampInUs(pkt->pts + pkt->convergence_duration,
        &(current_stream->time_base));
  } else if (pkt->duration != (int64_t)AV_NOPTS_VALUE &&
      pkt->duration != 0) {
    pAVPkt->end_pts = GetTimeStampInUs(pkt->pts + pkt->duration,
        &(current_stream->time_base));
  } else {
    pAVPkt->end_pts = pAVPkt->pts;
  }

  if ((pAVPkt->pts != kInvalidTimeStamp) && (duration_ > 0) &&
      (pAVPkt->pts > duration_ + start_time_)) {
    DATA_READER_ERROR("Bad pts, pts should not be bigger than duration!");
    pAVPkt->pts = kInvalidTimeStamp;
  }

  return true;
}

bool FFMPEGDataReader::applyPtsPatches(MmpBuffer *pAVPkt) {
  if (!pAVPkt) {
    return true;
  }

  if (current_video_ == (int32_t)pAVPkt->index) {
    if (pAVPkt->pts != kInvalidTimeStamp) {
      pAVPkt->pts = static_cast<TimeStamp>(static_cast<int64_t>(pAVPkt->pts)
          + video_pts_offset_);

      if (last_video_pts_ != kInvalidTimeStamp) {
        TimeStamp abs_pts_delta = 0;
        if (pAVPkt->pts <= last_video_pts_) {
          abs_pts_delta = last_video_pts_ - pAVPkt->pts;
        } else {
          abs_pts_delta = pAVPkt->pts - last_video_pts_;
        }

        if (abs_pts_delta > pts_delta_torl_) {
          int64_t new_offset = static_cast<int64_t>(last_video_pts_)
              - static_cast<int64_t>(pAVPkt->pts);
          video_pts_offset_ += new_offset;
          DATA_READER_RUNTIME("New video pts offset: "TIME_FORMAT"(%lld us)",
              TIME_ARGS(video_pts_offset_), video_pts_offset_);
          pAVPkt->pts = static_cast<TimeStamp>(static_cast<int64_t>(pAVPkt->pts) + new_offset);
        }
      }
      last_video_pts_ = pAVPkt->pts;
    } else if(video_pts_delta_ > 0) {
      // Make a fake pts for the packet with invalid pts, store it in last_video_pts_.
      // But this fake pts is not sent to OMX IL, decoder will make a more accurate
      // pts for it. This fake pts is only used to check pts discontinuity in player side.
      // However, this work only starts after a packet with valid pts comes.
      if (last_video_pts_ != kInvalidTimeStamp) {
        last_video_pts_ += video_pts_delta_;
      }
    }

    DATA_READER_VIDEO_FRAME("video frame pts flag %b, size %d pts = " TIME_FORMAT,
         pAVPkt->GetFlag(), pAVPkt->size, TIME_ARGS(pAVPkt->pts));

    if (b_getting_video_pts_after_seek_) {
      video_pts_after_seek_ = pAVPkt->pts;
      DATA_READER_RUNTIME("video_pts_after_seek_ = "TIME_FORMAT"(%lld us)",
          TIME_ARGS(video_pts_after_seek_), video_pts_after_seek_);
      b_getting_video_pts_after_seek_ = false;
    }
  }

  if (current_audio_ == (int32_t)pAVPkt->index) {
    if (pAVPkt->pts != kInvalidTimeStamp) {
      pAVPkt->pts = static_cast<TimeStamp>(static_cast<int64_t>(pAVPkt->pts)
          + video_pts_offset_);
    }

    DATA_READER_AUDIO_FRAME("audio frame pts flag %b, size %d pts = " TIME_FORMAT,
       pAVPkt->GetFlag(), pAVPkt->size, TIME_ARGS(pAVPkt->pts));

    if (b_getting_audio_pts_after_seek_) {
      audio_pts_after_seek_ = pAVPkt->pts;
      DATA_READER_RUNTIME("audio_pts_after_seek_ = "TIME_FORMAT"(%lld us)",
          TIME_ARGS(audio_pts_after_seek_), audio_pts_after_seek_);
      b_getting_audio_pts_after_seek_ = false;
    }
  }

  if (kInvalidTimeStamp == last_seen_pts_) {
    last_seen_pts_ = pAVPkt->pts;
  } else if (pAVPkt->pts > last_seen_pts_){
    last_seen_pts_ = pAVPkt->pts;
  }

  // If stream contains video, it will seek by video, so it will get video pts finally,
  // use video pts after seek as new start time.
  // If audio only stream, use audio pts after seek as new start time.
  if (b_pending_seek_) {
    TimeStamp seek_time = kInvalidTimeStamp;
    if (current_video_ >= 0) {
      if (!b_getting_video_pts_after_seek_) {
        if ((video_pts_after_seek_ != kInvalidTimeStamp) || (current_audio_ < 0)) {
          b_pending_seek_ = false;
          seek_time = video_pts_after_seek_;
        } else if (!b_getting_audio_pts_after_seek_) {
          b_pending_seek_ = false;
          seek_time = audio_pts_after_seek_;
        }
      }
    } else if (current_audio_ >= 0) {
      // Audio only stream
      if (!b_getting_audio_pts_after_seek_) {
        b_pending_seek_ = false;
        seek_time = audio_pts_after_seek_;
      }
    }

    if (!b_pending_seek_) {
      DATA_READER_RUNTIME("Got A/V data after seek. Set time to "
          TIME_FORMAT"(%lld us) after seek.", TIME_ARGS(seek_time), seek_time);
      cb_target_->OnSeekComplete(seek_time);
    }
  }

  return true;
}

bool FFMPEGDataReader::needFormat(AVCodecID codec_id) {
  bool ret = false;
  if ((AV_CODEC_ID_H264 == codec_id) ||
      (AV_CODEC_ID_RV30 == codec_id) ||
      (AV_CODEC_ID_RV40 == codec_id)) {
    ret = true;
  }
  return ret;
}

bool FFMPEGDataReader::hasPendingCmd() {
  bool ret = false;
  kdThreadMutexLock(mMsgQLock_);
  ret = !mMsgQ_.empty();
  kdThreadMutexUnlock(mMsgQLock_);
  return ret;
}

MmpError FFMPEGDataReader::onPlay() {
  DATA_READER_RUNTIME("recived DR start cmd.");
  dr_state_ = TS_DR_PLAY;
  DATA_READER_RUNTIME("DR change to play.");
  return MMP_NO_ERROR;
}

MmpError FFMPEGDataReader::onStop() {
  DATA_READER_RUNTIME("recived DR stop cmd.");
  dr_state_ = TS_DR_IDLE;

  return MMP_NO_ERROR;

}

MmpError FFMPEGDataReader::onSeek(MmpClockTime seek_time) {
  TimeStamp seek_pos_time = 0;
  muint32 seek_flags = 0x00;

  seek_pos_time = seek_time;
  seek_pos_time += start_time_;
  DATA_READER_RUNTIME("TS_DR_SEEK to "TIME_FORMAT"(%lld us), but really seek to "
      "stream "TIME_FORMAT"(%lld us) because start_time is "TIME_FORMAT"(%lld us).",
      TIME_ARGS(seek_time), seek_time, TIME_ARGS(seek_pos_time), seek_pos_time,
      TIME_ARGS(start_time_), start_time_);

  // By default, we want to seek forward.
  seek_flags = 0x00;

  // If seek at the end of the stream we will seek backward to save time
  if ((float)seek_pos_time / duration_ > 0.98) {
    seek_flags |= AVSEEK_FLAG_BACKWARD;
  }
  stream_seek(seek_pos_time, seek_flags);
  DATA_READER_RUNTIME("seek done!");

  return MMP_NO_ERROR;
}

bool FFMPEGDataReader::sendEOSBuffer() {
  MmpError ret = MMP_NO_ERROR;

  if (current_video_ >= 0) {
    MmpBuffer *video_eos = new MmpBuffer;
    if (video_eos) {
      video_eos->SetFlag(MmpBuffer::MMP_BUFFER_FLAG_EOS);
      FFStream* stream = &sub_streams_[current_video_];
      MmpPad* pad = stream->pad;
      if (M_LIKELY(pad)) {
        // TODO: EOS buffer has no size, should not be blocked, need confirm.
        ret = pad->Push(video_eos);
        DATA_READER_RUNTIME("queued EOS packet to video decoder");
      }
    }
  }

  if (current_audio_ >= 0) {
    MmpBuffer *audio_eos = new MmpBuffer;
    if (audio_eos) {
      audio_eos->SetFlag(MmpBuffer::MMP_BUFFER_FLAG_EOS);
      FFStream* stream = &sub_streams_[current_audio_];
      MmpPad* pad = stream->pad;
      if (M_LIKELY(pad)) {
        // TODO: EOS buffer has no size, should not be blocked, need confirm.
        ret = pad->Push(audio_eos);
        DATA_READER_RUNTIME("queued EOS packet to audio decoder");
      }
    }
  }

  return true;
}

void* FFMPEGDataReader::mainTaskEntry() {
  const int sleeptime = 10 * 1000; // 10ms
  MediaResult hr = kSuccess;
  int32_t err = 0;
  int32_t i = 0;
  AVStream *current_stream = NULL;

  ReaderCommand *cmd = NULL;

  if (hasPendingCmd()) {
process_cmd:
    cmd = *(mMsgQ_.begin());
    if (!cmd) {
      kdThreadMutexLock(mMsgQLock_);
      mMsgQ_.erase(mMsgQ_.begin());
      kdThreadMutexUnlock(mMsgQLock_);
      return NULL;
    }
    switch (cmd->mCommand) {
      case TS_DR_PLAY:
        onPlay();
        break;

      case TS_DR_STOP:
        onStop();
        break;

      case TS_DR_SEEK:
        onSeek(cmd->mParam);
        break;

      case TS_DR_EXIT:
        DATA_READER_RUNTIME("recived DR exit cmd.");
        /* Send command response */
        cmd->mCompleteSignal.setSignal();
        kdThreadMutexLock(mMsgQLock_);
        mMsgQ_.erase(mMsgQ_.begin());
        kdThreadMutexUnlock(mMsgQLock_);
        return NULL;
    }

    cmd->mCompleteSignal.setSignal();
    kdThreadMutexLock(mMsgQLock_);
    mMsgQ_.erase(mMsgQ_.begin());
    kdThreadMutexUnlock(mMsgQLock_);
  }

  /* Data feeding state */
  switch (dr_state_) {
    case TS_DR_PLAY: {
      if (mAVFormatContext_ == NULL) {
        DATA_READER_ERROR("No AVFormatContext found!");
        break;
      }
      MmpBuffer* ESPkt = new MmpBuffer;

      AVPacket packet;

      err = av_read_frame(mAVFormatContext_, &packet);
      current_stream = mAVFormatContext_->streams[packet.stream_index];

      // If new events comes, it has high priority.
      if (hasPendingCmd()) {
        DATA_READER_RUNTIME("Got new command during reading packet.");
        av_free_packet(&packet);
        delete ESPkt;
        goto process_cmd;
      }

      if (err == AVERROR(EAGAIN)) {
         av_free_packet(&packet);
         delete ESPkt;
         break;
      }

      if (err < 0) {
        DATA_READER_RUNTIME("eof, err = %d!", err);
        b_datareader_eos_ = true;
        av_free_packet(&packet);
        dr_state_ = TS_DR_IDLE;
        sendEOSBuffer();
        delete ESPkt;
        break;
      }

      // No need to process the not wanted data
      if ((packet.stream_index != current_audio_) &&
          (packet.stream_index != current_video_) &&
          (AVMEDIA_TYPE_SUBTITLE != current_stream->codec->codec_type)) {
        av_free_packet(&packet);
        delete ESPkt;
        break;
      }

      // Read all subtitles in memory in order to support subtitle switch.
      if ((AVMEDIA_TYPE_AUDIO == current_stream->codec->codec_type)) {
        if (packet.stream_index != current_audio_) {
          av_free_packet(&packet);
          delete ESPkt;
          break;
        }
      }

      // TODO: need to dup here, something not good
      // TODO: check memory leak here.
      av_dup_packet(&packet);

      ESPkt->index = packet.stream_index;
      if (packet.flags & AV_PKT_FLAG_KEY) {
        ESPkt->SetFlag(MmpBuffer::MMP_BUFFER_FLAG_KEY_FRAME);
      }

      calculatePTS(ESPkt, &packet);
      applyPtsPatches(ESPkt);

      if ((AVMEDIA_TYPE_VIDEO == current_stream->codec->codec_type)
          && (current_video_ == packet.stream_index)) {
        /* Check if the stream has I frame flag */
        if (!check_I_frames_ ) {
          if (packet.flags & AV_PKT_FLAG_KEY) {
            check_I_frames_ = true;
            b_seek_by_bytes_ = false;
            frames_without_I_frames_ = 0;
          } else {
            frames_without_I_frames_++;
            if (frames_without_I_frames_ >= 120) {
               check_I_frames_ = true;
               b_seek_by_bytes_ = true;
               frames_without_I_frames_ = 0;
            }
          }
        }
      }

      // If H264, format ES data.
      if (needFormat(current_stream->codec->codec_id) && mMRVLFormatter_ &&
          (mMRVLFormatter_->computeNewESLen(packet.data, packet.size) > 0)) {
        const uint32_t requiredLen = mMRVLFormatter_->computeNewESLen(packet.data, packet.size);
        ESPkt->data = (uint8_t *)malloc(requiredLen);
        if (ESPkt->data) {
          int32_t filledLength = mMRVLFormatter_->formatES(
              packet.data, packet.size, ESPkt->data, requiredLen);
          ESPkt->size = filledLength;
        }
        av_free_packet(&packet);
      } else {
        ESPkt->data = packet.data;
        ESPkt->size = packet.size;
      }

      FFStream* stream = &sub_streams_[packet.stream_index];
      MmpPad* pad = stream->pad;
      if (M_LIKELY(pad)) {
        MmpError ret = MMP_NO_ERROR;
        do {
          ret = pad->Push(ESPkt);
          if (MMP_PAD_FLOW_BLOCKED == ret) {
            usleep(sleeptime);
          } else {
            break;
          }
        } while (!hasPendingCmd());
        if (ret != MMP_NO_ERROR) {
          delete ESPkt;
          DATA_READER_RUNTIME("Push buffer failed with ret %d, hasPendingCmd = %d",
              ret, hasPendingCmd());
        }
      } else {
        delete ESPkt;
      }
    }
    break;

  case TS_DR_PAUSE:
  case TS_DR_IDLE:
    usleep(sleeptime); // 10ms
    break;

  default:
    usleep(sleeptime); // 10ms
    break;
  }

  return NULL;
}
}
