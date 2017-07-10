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

#include "demuxer/FFMPEGDataReader.h"
#include "datasource/HttpDataSource.h"
#include "datasource/FileDataSource.h"
#ifdef ENABLE_HLS
#include "datasource/HttpLiveDataSource.h"
#endif

#include "frameworks/mmp_frameworks.h"

#include "omx/MHALVideoDecoder.h"
#include "omx/MHALVideoRenderer.h"
#include "omx/MHALVideoProcessor.h"
#include "omx/MHALVideoScheduler.h"
#include "omx/MHALAudioDecoder.h"
#include "omx/MHALAudioRenderer.h"
#include "omx/MHALClock.h"

#include "CorePlayer.h"

#undef  LOG_TAG
#define LOG_TAG "CorePlayer"

namespace mmp {
extern uint64_t OnlineDebugBitMask;
volatile int32_t CorePlayer::instance_id_ = 0;

struct ContainerTypeEntry {
    const char *format;
    OMX_MEDIACONTAINER_FORMATTYPE containerType;
};

static const ContainerTypeEntry kContainerTable[] = {
  // { format type, container type }
  {"avi",                     OMX_FORMAT_AVI},
  {"matroska,webm",           OMX_FORMAT_MKV},
  {"mpegts",                  OMX_FORMAT_MPEG_TS},
  {"mpegtsraw",               OMX_FORMAT_MPEG_TS},
  {"mpeg",                    OMX_FORMAT_MPEG_ES}, // Is it correct?
  {"asf",                     OMX_FORMAT_ASF},
  {"wav",                     OMX_FORMAT_WAVE},
  {"mov,mp4,m4a,3gp,3g2,mj2", OMX_FORMAT_MP4},
  {"flv",                     OMX_FORMAT_FLV},
  {"rm",                      OMX_FORMAT_RM},
  {"mp3",                     OMX_FORMAT_MP3},
  {"ogg",                     OMX_FORMAT_OGG},
  {"aac",                     OMX_FORMAT_AAC},
};

OMX_MEDIACONTAINER_FORMATTYPE getContainerType(const char *format) {
  for (int i = 0; i < NELEM(kContainerTable); ++i) {
    if (!strcmp(format, kContainerTable[i].format)) {
      return kContainerTable[i].containerType;
    }
  }
  return OMX_FORMATMax;
}


OMX_BOOL CorePlayer::OnOmxEvent(MHAL_EVENT_TYPE EventCode, OMX_U32 nData1,
    OMX_U32 nData2, OMX_PTR pEventData) {
  muint32 resolution = 0;
  int32_t displayedFps = 0;
  switch (EventCode) {
    case EVENT_AUDIO_EOS:
      CORE_PLAYER_RUNTIME("receive audio EOS");
      if (hasAudio() && (mMHALAudioDecoder_ != NULL)) {
        audio_complete_ = true;
      }
      if ((hasAudio() && (mMHALAudioDecoder_ != NULL) && !audio_complete_) ||
          (hasVideo() && (mMHALVideoDecoder_ != NULL) && !video_complete_)) {
        // There is still one stream not EOS.
      } else if (cb_target_) {
        CORE_PLAYER_RUNTIME("EOS received!!!");
        cb_target_->sendMsg(MSG_EOS , NULL);
      }
      break;

    case EVENT_VIDEO_EOS:
      CORE_PLAYER_RUNTIME("receive video EOS");
      if (hasVideo() && (mMHALVideoDecoder_ != NULL)) {
        video_complete_ = true;
      }
      if ((hasAudio() && (mMHALAudioDecoder_ != NULL) && !audio_complete_) ||
          (hasVideo() && (mMHALVideoDecoder_ != NULL) && !video_complete_)) {
        // There is still one stream not EOS.
      } else if (cb_target_) {
        CORE_PLAYER_RUNTIME("EOS received!!!");
        cb_target_->sendMsg(MSG_EOS , NULL);
      }
      break;

    case EVENT_COMMAND_COMPLETE:
      if (OMX_CommandStateSet == nData1) {
        if (mStateCount_ > 0) {
          mStateCount_--;
        }
        if (0 == mStateCount_) {
          CORE_PLAYER_RUNTIME("All components finished state change.");
          mStateSignal_->setSignal();
        }
      }
      if (OMX_CommandFlush == nData1) {
        if (mFlushCount_ > 0) {
          mFlushCount_--;
        }
        if (0 == mFlushCount_) {
          CORE_PLAYER_RUNTIME("All components finished flush.");
          mFlushSignal_->setSignal();
        }
      }
      break;
    case EVENT_FPS_INFO:
      displayedFps = nData1 & 0xFFFF;
      if ((displayedFps > 0) && (!mVideoRenderingStarted_)) {
        mVideoRenderingStarted_ = true;
        if (cb_target_) {
          cb_target_->sendMsg(MSG_RENDERING_START , NULL);
        }
      }
      break;
    case EVENT_VIDEO_RESOLUTION:
      resolution = (nData1 << 16) | (nData2);
      if (cb_target_) {
        cb_target_->sendMsg(MSG_RES_CHANGE, &resolution);
      }
      break;
    default:
      break;
  }
  return OMX_TRUE;
}

MediaResult CorePlayer::OnDataSourceEvent(DataSourceEvent what, int extra) {
  switch (what) {
    case DS_HLS_PLAYING_BANDWIDTH:
      if (cb_target_) {
        cb_target_->sendMsg(MSG_PLAYING_BANDWIDTH, &extra);
      }
      break;

    case DS_HLS_REALTIME_SPEED:
      if (cb_target_) {
        cb_target_->sendMsg(MSG_HLS_REALTIME_SPEED, &extra);
      }
      break;

    case DS_HLS_DOWNLOAD_ERROR:
      CORE_PLAYER_ERROR("%s() line %d, received error from data source, errorno = %d",
          __FUNCTION__, __LINE__, extra);
      if (cb_target_) {
        cb_target_->sendMsg(MSG_ERROR, &extra);
      }
      break;

    case DS_HLS_DOWNLOAD_PLAYLIST_ERROR:
      if (cb_target_) {
        int errorno = MRVL_PLAYER_ERROR_FAILED_GET_HLS_PLAYLIST;
        cb_target_->sendMsg(MSG_ERROR, &errorno);
      }
      break;

    case DS_HLS_DOWNLOAD_TS_ERROR:
      if (cb_target_) {
        int errorno = MRVL_PLAYER_ERROR_FAILED_GET_HLS_TS;
        cb_target_->sendMsg(MSG_ERROR, &errorno);
      }
      break;

    case DS_HLS_GOT_IP_ADDRESS:
      if (cb_target_) {
        // extra is already an pointer.
        cb_target_->sendMsg(MSG_HLS_IP_ADDRESS, (void *)extra);
      }
      break;

    default:
      CORE_PLAYER_RUNTIME("Unknown data source event:%d", what);
      break;
  }

  return kSuccess;
}

bool CorePlayer::OnSeekComplete(TimeStamp seek_time) {
  if (kInvalidTimeStamp == seek_time) {
    seek_time = seek_target_ + file_info.start_time;
  }

  CORE_PLAYER_RUNTIME("Seek complete after got data. Kick off clock to "
      TIME_FORMAT"(%lld us) after seek.",
      TIME_ARGS(seek_time), seek_time);

  buffering_start_pts_ = seek_time;
  if (mMmpClock_) {
    mMmpClock_->startClock(seek_time);
    if (STATE_PLAYING == state_) {
      if (isDataBuffered()) {
        // Send buffering 100% if data is buffered.
        if (cb_target_) {
          int buffering_level = 100;
          cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
        }
        mMmpClock_->setScale(1.0);
      } else {
        b_pending_start_ = true;
        if (cb_target_) {
          int buffering_level = 0;
          cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
        }
      }
    } else {
      mMmpClock_->setScale(0.0);
      // Buffering start after seek.
      b_pending_start_ = true;
      if (cb_target_) {
        int buffering_level = 0;
        cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
      }
    }
  }

  if (SOURCE_HLS == getDataSourceType()) {
    hls_position_offset_ = hls_seek_time_ - seek_time;
    CORE_PLAYER_RUNTIME("hls_position_offset_ = %s"TIME_FORMAT"(%lld us)",
        hls_position_offset_ > 0 ? "" : "-",
        TIME_ARGS(hls_position_offset_ > 0 ? hls_position_offset_ : -hls_position_offset_),
        hls_position_offset_);
  }

  if (cb_target_) {
    cb_target_->sendMsg(MSG_SEEK_COMPLETE, NULL);
  }
  return true;
}

MmpError CorePlayer::onSubtitleCome(void *parcel) {
  if (cb_target_ && sending_subtitle_) {
    cb_target_->sendMsg(MSG_TIMED_TEXT, parcel);
  }
  return MMP_NO_ERROR;
}

void CorePlayer::initVariables() {
  state_ = STATE_IDLE;

  audio_id_ = -1;
  video_id_ = -1;
  subtitle_id_ = -1;
  current_video_ = -1;
  current_audio_ = -1;

  video_complete_ = false;
  audio_complete_ = false;

  pending_seek_ = false;
  need_update_offset_ = false;
  seek_target_ = 0;

  b_pending_start_ = false;
  buffering_start_pts_ = kInvalidTimeStamp;

  hls_position_offset_ = 0;
  hls_seek_time_ = 0;
  b_is_underrun_ = false;
  b_underrun_status_ = false;

  mStateCount_ = 0;
  mFlushCount_ = 0;

  sending_subtitle_ = true;

  omx_tunnel_mode_ = OMX_TUNNEL;

  prepared_ = false;

  ext_sub_count_ = 0;

  stream_info_probed_ = false;

  for (int i = 0; i < ext_srt_dmx_list_.size(); i++) {
    delete ext_srt_dmx_list_[i];
    ext_srt_dmx_list_[i] = NULL;
  }
  ext_srt_dmx_list_.clear();
}

void CorePlayer::recycleResource() {
  CORE_PLAYER_API("Enter");

  if (mRawDataSource_) {
    mRawDataSource_->registerCallback(NULL);
    delete mRawDataSource_;
    mRawDataSource_ = NULL;
    CORE_PLAYER_API("mRawDataSource_ is deleted");
  }

  if (mRawDataReader_) {
    delete mRawDataReader_;
    mRawDataReader_ = NULL;
    CORE_PLAYER_API("mRawDataReader_ is deleted");
  }

  if (url_) {
    free(url_);
    url_ = NULL;
  }

  if (source_fd_ >= 0) {
    ::close(source_fd_);
    source_fd_ = -1;
  }

  while (!subtitle_engines_.empty()) {
    MmpElement *engine = *(subtitle_engines_.begin());
    if (engine) {
      delete engine;
      engine = NULL;
    }
    subtitle_engines_.erase(subtitle_engines_.begin());
  }

  // It is a proper way to put all omx components to Loaded state, then destroy
  // them one by one, because some of them may have tunnel between each other.
  if (!mMHALOmxComponents_.empty()) {
    CORE_PLAYER_RUNTIME("Destroy omx components.");
    mStateCount_ = mMHALOmxComponents_.size();
    changeOmxCompsState(OMX_StateLoaded, false);
    while (!mMHALOmxComponents_.empty()) {
      MHALOmxComponent *comp = *(mMHALOmxComponents_.begin());
      if (comp) {
        delete comp;
        comp = NULL;
      }
      mMHALOmxComponents_.erase(mMHALOmxComponents_.begin());
    }
    mMHALClock_ = NULL;
    mMHALAudioDecoder_ = NULL;
    mMHALAudioRenderer_ = NULL;
    mMHALVideoDecoder_ = NULL;
    CORE_PLAYER_RUNTIME("All omx components are destroyed.");
  }


#ifdef ENABLE_RESOURCE_MANAGER
  ReleaseResource();
#endif

  if (mStateSignal_) {
    delete mStateSignal_;
    mStateSignal_ = NULL;
  }
  if (mFlushSignal_) {
    delete mFlushSignal_;
    mFlushSignal_ = NULL;
  }

  if (mAndroidNativeWindowRenderer_) {
    delete mAndroidNativeWindowRenderer_;
    mAndroidNativeWindowRenderer_ = NULL;
    CORE_PLAYER_RUNTIME("AndroidNativeWindowRenderer is destroyed.");
  }
  if (mAndroidAudioRenderer_) {
    delete mAndroidAudioRenderer_;
    mAndroidAudioRenderer_ = NULL;
    CORE_PLAYER_RUNTIME("AndroidAudioRenderer is destroyed.");
  }

  if (OMX_TUNNEL == omx_tunnel_mode_) {
    // In tunnel mode, clock is released as a MHALOmxComponent.
    delete mMmpClock_;
    mMmpClock_ = NULL;
    CORE_PLAYER_RUNTIME("MmpClock is destroyed.");
  }

  CORE_PLAYER_API("Leave");
}

CorePlayer::CorePlayer(CorePlayerCbTarget *target)
  : url_(NULL),
    source_fd_(-1),
    source_offset_(0),
    source_length_(0),
    headers_(NULL),
    mRawDataSource_(NULL),
    data_source_type_(SOURCE_INVALID),
    mAVInputFormat_(NULL),
    mRawDataReader_(NULL),
    cb_target_(target),
    mMHALClock_(NULL),
    mMHALAudioDecoder_(NULL),
    mMHALAudioRenderer_(NULL),
    mMHALVideoDecoder_(NULL),
    mAndroidNativeWindowRenderer_(NULL),
    mAndroidAudioRenderer_(NULL),
    mVideoRenderingStarted_(false),
    mMmpClock_(NULL) {
  // Give each instance a unique log tag.
  sprintf(log_tag_, "CorePlayer%d", instance_id_);
  android_atomic_inc(&instance_id_);

  CORE_PLAYER_API("begin to create CorePlayer");

  initVariables();

  memset(&file_info, 0x00, sizeof(MEDIA_FILE_INFO));

#ifdef ENABLE_RESOURCE_MANAGER
  mResourceClientHandle_ = 0;
  app_resource_handle_ = 0;
  audio_decoder_handle_ = 0;
  vpp_resource_handle_ = 0;
  video_decoder_handle_ = 0;
  mMrvlMediaPlayerClient_ = NULL;
  audio_channel_ = 0;
  video_channel_ = 0;
  mResourceReadySignal_ = new kdCondSignal();
#endif

  mStateSignal_ = new kdCondSignal();
  mFlushSignal_ = new kdCondSignal();
  if (!mStateSignal_ || !mFlushSignal_) {
    OMX_LOGE("Out of memory!");
  }

  mNativeWindow_ = NULL;

  CORE_PLAYER_API("CorePlayer is created");
}


CorePlayer::~CorePlayer() {
  CORE_PLAYER_API("begin to destroy CorePlayer");

  initVariables();
  recycleResource();

  CORE_PLAYER_API("CorePlayer is destroyed");
}

MediaResult CorePlayer::Play(double rate) {
  CORE_PLAYER_API("play rate = %f", rate);

  if (rate == 0 || rate > 100 || rate < -100) {
    CORE_PLAYER_ERROR("%s() line %d, rate(%f) is invalid", __FUNCTION__, __LINE__, rate);
    return kInvalidArgument;
  }

  if (state_ == STATE_PLAYING) {
    CORE_PLAYER_API("already playing, return from here");
    return kSuccess;
  }

  vector<TimedTextDemux *>::iterator it;
  for (it = ext_srt_dmx_list_.begin() ; it != ext_srt_dmx_list_.end(); ++it) {
    (*it)->start();
  }

  if (GetSubtitleStreamCount() > 0 && subtitle_id_ == -1) {
    SelectSubtitleStream(0);
  }

  // For online stream, seekTo(0) may be called after prepared.
  // So start data source after start().
  if (mRawDataSource_ && data_source_type_ == SOURCE_HTTP) {
    // the source was stopped, so start it again
    mRawDataSource_->startDataSource();
  }

  CORE_PLAYER_API("start DR thread...");
  mRawDataReader_->Start(1.0);

  buffering_start_pts_ = file_info.start_time;
  if (mMmpClock_) {
    // Kick off clock and start playback.
    CORE_PLAYER_API("Kick off clock to "TIME_FORMAT"(%lld us) when begin playback.",
        TIME_ARGS(file_info.start_time), file_info.start_time);
    mMmpClock_->startClock(file_info.start_time);
    if (isDataBuffered()) {
      // Send buffering 100% if data is buffered.
      if (cb_target_) {
        int buffering_level = 100;
        cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
      }
      mMmpClock_->setScale(1.0);
    } else {
      b_pending_start_ = true;
      if (cb_target_) {
        int buffering_level = 0;
        cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
      }
    }
  }

  state_ = STATE_PLAYING;
  CORE_PLAYER_API("now it starts playback with rate = %.2f", rate);
  return kSuccess;
}

MediaResult CorePlayer::probeStreamInfo() {
  MediaResult ret = kUnexpectedError;

  file_info.duration = 0;

  createDataSource();

  // Firstly, we use FFmpeg to probe it.
  mRawDataReader_ = new FFMPEGDataReader(this);

  if (mRawDataSource_ && mRawDataSource_->isPrepared()) {
    mAVInputFormat_ = mRawDataSource_->getAVInputFormat();
    ret = mRawDataReader_->Probe(url_, (void*)mRawDataSource_->getByteIOContext(),
        (void*)mAVInputFormat_, &file_info);
  } else if ((!mRawDataSource_) && (NULL != url_)) {
    // If source is not HLS, Http video, or local file, let ffmpeg to decide what to do.
    ret = mRawDataReader_->Probe(url_, &file_info);
  }

  if (ret != kSuccess) {
    CORE_PLAYER_RUNTIME("We meet error when probe file");
  } else {
    stream_info_probed_ = true;
    CORE_PLAYER_RUNTIME("Successfully parsed the stream info:");
    CORE_PLAYER_RUNTIME("video stream counts = %d", file_info.supported_video_counts);
    CORE_PLAYER_RUNTIME("audio stream counts = %d", file_info.supported_audio_counts);
    CORE_PLAYER_RUNTIME("internal subtitle counts = %d",
        file_info.supported_subtitle_counts);
    if (file_info.supported_video_counts > 0) {
      MmpPad* video_pad = mRawDataReader_->GetSrcPadByPrefix("video");
      MmpCaps& video_caps = video_pad->GetCaps();
      mint32 width = 0;
      mint32 height = 0;
      video_caps.GetInt32("width",&width);
      video_caps.GetInt32("height",&height);
      // Only 1080P is supported at the moment.
      if ((width > 1920) || (height > 1088)) {
        CORE_PLAYER_ERROR("Resolution is too big(%d X %d), only 1080P is supported "
            "at the moment.", width, height);
        return kUnexpectedError;
      }
      muint32 resolution = (width << 16) | (height);
      if (cb_target_) {
        cb_target_->sendMsg(MSG_RES_CHANGE, &resolution);
      }
    }
  }

  return ret;
}

MediaResult CorePlayer::Prepare() {
  CORE_PLAYER_API("ENTER");
  MediaResult ret = kUnexpectedError;

  // Send buffering 0% when start preparing.
  if (cb_target_) {
    int buffering_level = 0;
    cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
  }

  if (!stream_info_probed_) {
    ret = probeStreamInfo();
    if (ret != kSuccess) {
      goto error;
    }
  }

  if (file_info.supported_video_counts > 0) {
    MmpPad* video_pad = mRawDataReader_->GetSrcPadByPrefix("video");
    MmpCaps& video_caps = video_pad->GetCaps();
    mint32 width = 0;
    mint32 height = 0;
    video_caps.GetInt32("width",&width);
    video_caps.GetInt32("height",&height);
    // Only 1080P is supported at the moment.
    if ((width > 1920) || (height > 1088)) {
      CORE_PLAYER_ERROR("Resolution is too big(%d X %d), only 1080P is supported "
          "at the moment.", width, height);
      goto error;
    }
    muint32 resolution = (width << 16) | (height);
    if (cb_target_) {
      cb_target_->sendMsg(MSG_RES_CHANGE, &resolution);
    }
  }

  // check the supported stream info and pop messages up.
  if (file_info.supported_video_counts > 0 || file_info.supported_audio_counts > 0) {
    if (file_info.supported_video_counts == 0 && file_info.total_video_counts > 0) {
      CORE_PLAYER_RUNTIME("video unsupported, bad luck");
      cb_target_->sendMsg(MSG_VIDEO_NOTSUPPORT);
    }
    if (file_info.supported_audio_counts == 0 && file_info.total_audio_counts > 0) {
      CORE_PLAYER_RUNTIME("audio unsupported, bad luck");
      cb_target_->sendMsg(MSG_AUDIO_NOTSUPPORT);
    }
  } else {
    cb_target_->sendMsg(MSG_UNSUPPORT);
    CORE_PLAYER_ERROR("No video or audio can play in this stream");
    goto error;
  }

  if (file_info.bit_rate == 0) {
    int duration_s = file_info.duration/1000000000;
    if (file_info.file_size != 0 && duration_s != 0) {
      file_info.bit_rate = file_info.file_size/duration_s;
      CORE_PLAYER_RUNTIME("calculate the bitrate is %d", file_info.bit_rate);
    }
    CORE_PLAYER_RUNTIME("The bitrate is %d", file_info.bit_rate);
  }

  state_ = STATE_IDLE;

  //Create data reading thread
  CORE_PLAYER_RUNTIME("creating DataReader...");
  mRawDataReader_->Init();

  // Send buffering 100% to application.
  OnDataSourceEvent(DS_HLS_DOWNLOAD_PROGRESS_IN_PERCENT, 10000);

  CORE_PLAYER_RUNTIME("file_info.start_time = "TIME_FORMAT, TIME_ARGS(file_info.start_time));

#ifdef ENABLE_RESOURCE_MANAGER
  if (kSuccess != RequestResource() || !gotAllResource()) {
    CORE_PLAYER_ERROR("Unfortunately, failed to get resource from ResourceManager!");
    goto error;
  }
#endif

  if (OMX_TUNNEL == omx_tunnel_mode_) {
    mMHALClock_ = new MHALClock(this);
    if (mMHALClock_) {
      mMHALClock_->setUp();
      mMHALOmxComponents_.push_back(mMHALClock_);
    }
    mMmpClock_ = mMHALClock_;
  } else if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    mMmpClock_ = new MmpSystemClock();
  }

  if (hasAudio()) {
    if (MMP_NO_ERROR != buildAudioPipeline()) {
      goto error;
    }
  }

  if (hasVideo()) {
    if (MMP_NO_ERROR != buildVideoPipeline()) {
      goto error;
    }
  }

  if (hasSubtitle()) {
    if (MMP_NO_ERROR != buildSubtitlePipeline()) {
      goto error;
    }
  }

  // Only all pipelines are setup can player play the stream.
  if ((hasAudio() && !mMHALAudioDecoder_) || (hasVideo() && !mMHALVideoDecoder_)) {
    CORE_PLAYER_ERROR("No audio/video can play!");
    while (!mMHALOmxComponents_.empty()) {
      MHALOmxComponent *comp = *(mMHALOmxComponents_.begin());
      delete comp;
      comp = NULL;
      mMHALOmxComponents_.erase(mMHALOmxComponents_.begin());
    }
    mMHALClock_ = NULL;
    mMHALAudioDecoder_ = NULL;
    mMHALAudioRenderer_ = NULL;
    mMHALVideoDecoder_ = NULL;
    goto error;
  }

  if (mMHALVideoDecoder_ && mAVInputFormat_) {
    OMX_MEDIACONTAINER_FORMATTYPE type = getContainerType(mAVInputFormat_->name);
    CORE_PLAYER_RUNTIME("format %s, type %d", mAVInputFormat_->name, type);
    mMHALVideoDecoder_->setContainerType(type);
  }

  if (mMHALAudioDecoder_ && mAVInputFormat_) {
    OMX_MEDIACONTAINER_FORMATTYPE type = getContainerType(mAVInputFormat_->name);
    CORE_PLAYER_RUNTIME("format %s, type %d", mAVInputFormat_->name, type);
    mMHALAudioDecoder_->setContainerType(type);
  }

  // The calling sequence of state change from OMX_StateLoaded to OMX_StateIdle
  // must be guaranteed. Or crash happens.
  CORE_PLAYER_RUNTIME("put omx components to idle state.");
  for (uint32_t idx = 0; idx < mMHALOmxComponents_.size(); idx++) {
    MHALOmxComponent *comp = mMHALOmxComponents_.at(idx);
    if (comp) {
      comp->setOmxState(OMX_StateIdle);
    }
  }

  // Tell data reader the stream index.
  mRawDataReader_->SetCurrentStream(current_video_, current_audio_);

  // Put pipeline running though no data is sending to it before start() is called.
  mStateCount_ = mMHALOmxComponents_.size();
  changeOmxCompsState(OMX_StateExecuting, false);

  if (mMmpClock_) {
    // Hold the clock until start playing.
    mMmpClock_->setScale(0.0);
  }

  CORE_PLAYER_RUNTIME("omx begin push input buffer");
  // Begin transfer data.
  if (mMHALAudioDecoder_) {
    mMHALAudioDecoder_->pushCodecConfigData();
    mMHALAudioDecoder_->canSendBufferToOmx(OMX_TRUE);
  }
  if (mMHALVideoDecoder_) {
    mMHALVideoDecoder_->pushCodecConfigData();
    mMHALVideoDecoder_->canSendBufferToOmx(OMX_TRUE);
  }

  prepared_ = true;

  CORE_PLAYER_RUNTIME("Now framework is prepared to play the stream");
  return kSuccess;

error:
  if (mRawDataReader_) {
    delete mRawDataReader_;
    mRawDataReader_ = NULL;
  }
  initVariables();
  return kUnexpectedError;
}


MediaResult CorePlayer::Pause() {
  TimeStamp playpos = 0;

  CORE_PLAYER_API("Enter");
  if (state_ == STATE_PAUSED) {
    CORE_PLAYER_API("already in pause state");
    return kSuccess;
  }

  if (state_ == STATE_PLAYING) {
    if (mMmpClock_) {
      mMmpClock_->setScale(0.0);
    }

    state_ = STATE_PAUSED;
  }

  CORE_PLAYER_API("LEAVE");
  return kSuccess;
}

// When stop, stop the pipeline totally.
// Then call start after stop, it is the same as the first time call start.
MediaResult CorePlayer::Stop() {
  CORE_PLAYER_API("Enter");

  if (state_ == STATE_STOPPED) {
    CORE_PLAYER_API("Leave, already stopped");
    return kSuccess;
  }

  // Firstly we stop reading data, so it won't send data to engine anymore.
  // Stop reading data firstly can avoid send data to PE after calling PE stop.
  if (mRawDataSource_) {
    // We need this so that in case of Internet streaming, FFMPEGDRThreadProc() won't block.
    mRawDataSource_->stopDataSource();
    CORE_PLAYER_RUNTIME("DataSource has stopped");
  }
  if (mRawDataReader_) {
    if ((SOURCE_FILE == data_source_type_) && (file_info.duration > 0)) {
      mRawDataReader_->Seek(0);
      CORE_PLAYER_RUNTIME("On stop, DataReader seeked to beginning of stream");
    }
    mRawDataReader_->Stop();
    CORE_PLAYER_RUNTIME("DataReader has stopped");
  }

  // Stop clock.
  if (mMmpClock_) {
    CORE_PLAYER_RUNTIME("stop clock firstly");
    mMmpClock_->setScale(0.0);
    mMmpClock_->stopClock();
  }
  // Stop sending audio data.
  if (mMHALAudioDecoder_) {
    CORE_PLAYER_RUNTIME("Stop sending audio data.");
    mMHALAudioDecoder_->canSendBufferToOmx(OMX_FALSE);
  }
  // Stop sending video data.
  if (mMHALVideoDecoder_) {
    CORE_PLAYER_RUNTIME("Stop sending video data.");
    mMHALVideoDecoder_->canSendBufferToOmx(OMX_FALSE);
  }

  CORE_PLAYER_RUNTIME("put omx components to idle state.");
  mStateCount_ = mMHALOmxComponents_.size();
  changeOmxCompsState(OMX_StateIdle, false);

  CORE_PLAYER_RUNTIME("Put omx components to loaded, so that they will release resource.");
  mStateCount_ = mMHALOmxComponents_.size();
  changeOmxCompsState(OMX_StateLoaded, false);

  state_ = STATE_STOPPED;
  CORE_PLAYER_API("Leave");
  return kSuccess;
}

// When reset, the whole pipeline is destroyed.
MediaResult CorePlayer::Reset(){
  CORE_PLAYER_API("Enter");

  Stop();

  recycleResource();

  CORE_PLAYER_API("Leave");
  return kSuccess;
}

MediaResult CorePlayer::Resume(double rate) {

  CORE_PLAYER_API("Enter, resume playback with rate = %f", rate);

  if (state_ == STATE_PLAYING) {
    CORE_PLAYER_API("already playing, return from here");
    return kSuccess;
  }

  if (b_pending_start_) {
    CORE_PLAYER_API("Player is buffering maybe after first time play or after seek");
    state_ = STATE_PLAYING;
    return kSuccess;
  }

  if (mMmpClock_) {
    mMmpClock_->setScale(1.0);
  }

  state_ = STATE_PLAYING;

  CORE_PLAYER_API("LEAVE");
  return kSuccess;
}

TimeStamp CorePlayer::GetPosition() {
  TimeStamp cur_pos = 0;
  if (mMmpClock_) {
    cur_pos = mMmpClock_->GetTime();
  }

  // Patch for HLS: sometimes pts will start from 0 again during playback.
  cur_pos += hls_position_offset_;


  return (cur_pos > file_info.start_time) ? (cur_pos - file_info.start_time) : 0;
}


TimeStamp CorePlayer::GetDuration() {
  int64_t hls_duration_us = 0;

  if (SOURCE_HLS == getDataSourceType()) {
    if (mRawDataSource_ && mRawDataSource_->getDurationUs(&hls_duration_us)) {
      if (hls_duration_us < 0) {
       // It is live content.
       return 0;
      } else {
       return hls_duration_us;
      }
    }
  } else {
    return file_info.duration;
  }
  return 0;
}

bool CorePlayer::changeOmxCompsState(OMX_STATETYPE state, bool without_clock) {
  CORE_PLAYER_RUNTIME("Enter, change %d comp's state to %s(%d).",
      mStateCount_, omxState2String(state), state);
  if (0 == mStateCount_) {
    return true;
  }
  mStateSignal_->resetSignal();

  for (uint32_t idx = 0; idx < mMHALOmxComponents_.size(); idx++) {
    MHALOmxComponent *comp = mMHALOmxComponents_.at(idx);
    if (comp) {
      if (comp == mMHALClock_) {
        if (!without_clock) {
          comp->setOmxStateAsync(state);
        }
      } else {
        comp->setOmxStateAsync(state);
      }
    }
  }

  mStateSignal_->waitSignal();
  CORE_PLAYER_RUNTIME("Exit.");
  return true;
}

bool CorePlayer::flushOmxComps(bool without_clock) {
  CORE_PLAYER_RUNTIME("Enter, flush %d comps.", mFlushCount_);
  if (0 == mFlushCount_) {
    return true;
  }
  mFlushSignal_->resetSignal();

  for (uint32_t idx = 0; idx < mMHALOmxComponents_.size(); idx++) {
    MHALOmxComponent *comp = mMHALOmxComponents_.at(idx);
    if (comp) {
      if (comp == mMHALClock_) {
        if (!without_clock) {
          comp->flushAsync();
        }
      } else {
        comp->flushAsync();
      }
    }
  }

  mFlushSignal_->waitSignal();
  CORE_PLAYER_RUNTIME("Exit.");
  return true;
}

// Seek Event Sequence:
// 1. Stop clock componets;
// 2. Flush all components in A/V pipeline;
// 3. Seek to required position;
// 4. Start reading data;
// 5. Start clock components with video_pts if have video.
MediaResult CorePlayer::SetPosition(TimeStamp posInUs) {
  MediaResult ret = kSuccess;
  CORE_PLAYER_RUNTIME("ENTER, seek to position "TIME_FORMAT"(%lld us)",
      TIME_ARGS(posInUs), posInUs);

  if (state_ == STATE_STOPPED) {
    CORE_PLAYER_API("Player is in STATE_STOPPED, can't do seek.");
    return kSuccess;
  }

  TimeStamp duration_us = GetDuration();
  if (mRawDataSource_ && mRawDataSource_->isSeekable()) {
    if (duration_us <= 0) {
      CORE_PLAYER_ERROR("It can't seek, maybe it is live stream.");
      if (cb_target_) {
        // Send buffering 100% to application.
        OnDataSourceEvent(DS_HLS_DOWNLOAD_PROGRESS_IN_PERCENT, 10000);
        cb_target_->sendMsg(MSG_SEEK_COMPLETE, NULL);
      }
      return kSuccess;
    }
  }

  if ((0 == duration_us) || (posInUs > duration_us)) {
    CORE_PLAYER_ERROR("Invalid seek position "TIME_FORMAT"(%lld us),"
        " duration is "TIME_FORMAT"(%lld us)",
        TIME_ARGS(posInUs), posInUs, TIME_ARGS(file_info.duration), file_info.duration);
    if (cb_target_) {
      cb_target_->sendMsg(MSG_SEEK_COMPLETE, NULL);
    }
    return kSuccess;
  }

  // Send buffering 0% when seek.
  if (cb_target_) {
    int buffering_level = 0;
    cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
  }

  // Update seek target.
  seek_target_ = posInUs;

  // Firstly we stop reading data, so it won't send data to engine anymore.
  // Stop reading data firstly can avoid send data to PE after calling PE stop.
  if (mRawDataSource_) {
    // We need this so that in case of Internet streaming, FFMPEGDRThreadProc() won't block.
    mRawDataSource_->stopDataSource();
  }

  // After DataReader has stopped, seek won't block.
  ret = mRawDataReader_->Stop();
  if (kSuccess != ret) {
    CORE_PLAYER_ERROR("Failed to stop data reader!");
    return ret;
  }
  for (vector<TimedTextDemux *>::iterator it = ext_srt_dmx_list_.begin();
    it != ext_srt_dmx_list_.end(); ++it) {
    (*it)->stop();
  }

  // If pipeline is not ready, just seek data source.
  if (state_ > STATE_IDLE) {
    CORE_PLAYER_RUNTIME("Flush pipeline.");
    if (mMHALVideoDecoder_) {
      mMHALVideoDecoder_->canSendBufferToOmx(OMX_FALSE);
    }
    if (mMHALAudioDecoder_) {
      mMHALAudioDecoder_->canSendBufferToOmx(OMX_FALSE);
    }

    // Stop clock component.
    if (mMmpClock_) {
      mMmpClock_->setScale(0.0);
      mMmpClock_->stopClock();
    }

    // Pause A/V pipeline.
    mStateCount_ = mMHALOmxComponents_.size() - 1;
    changeOmxCompsState(OMX_StatePause, true);

    // Flush A/V pipeline.
    mFlushCount_ = mMHALOmxComponents_.size() - 1;
    flushOmxComps(true);

    // Un-pause A/V pipeline.
    mStateCount_ = mMHALOmxComponents_.size() - 1;
    changeOmxCompsState(OMX_StateExecuting, true);

    vector<MmpElement*>::iterator it;
    for (it = subtitle_engines_.begin(); it != subtitle_engines_.end(); it++) {
      MmpElement* engine = *it;
      MmpEvent flush_event(MmpEvent::MMP_EVENT_FLUSH_START);
      engine->SendEvent(&flush_event);
    }

    if (mMHALVideoDecoder_) {
      mMHALVideoDecoder_->canSendBufferToOmx(OMX_TRUE);
    }
    if (mMHALAudioDecoder_) {
      mMHALAudioDecoder_->canSendBufferToOmx(OMX_TRUE);
    }
  }

  CORE_PLAYER_RUNTIME("Seek data source to new position.");

  pending_seek_ = true;

  // If data source can't do seek, let FFMPEG handle it.
  if (!mRawDataSource_ || !(mRawDataSource_ && mRawDataSource_->isSeekable())) {
    if (mRawDataSource_ && data_source_type_ == SOURCE_HTTP) {
      // the source was stopped, so start it again
      mRawDataSource_->startDataSource();
    }
    ret = mRawDataReader_->Seek(posInUs);
    if (kSuccess != ret) {
      CORE_PLAYER_ERROR("Failed to seek data reader!");
      return ret;
    }
  } else if (mRawDataSource_ && mRawDataSource_->isSeekable()) {
    // Flush the data that FFMPEG has read.
    mRawDataReader_->FlushDataReader();
    // Seek HLS.
    mRawDataSource_->seekToUs(posInUs);
    // If HLS has reached EOS, eof_reached is set to 1. We have to clean eof_reached
    // so that after seek ffmpeg_dr can continue to read data.
    if (mRawDataSource_->getByteIOContext()) {
      mRawDataSource_->getByteIOContext()->eof_reached = 0;
    }
  }

  for (vector<TimedTextDemux *>::iterator it = ext_srt_dmx_list_.begin();
    it != ext_srt_dmx_list_.end(); ++it) {
    (*it)->seek(0);
  }

  if (SOURCE_HLS == getDataSourceType()) {
    hls_position_offset_ = 0;
    mRawDataSource_->getLastSeekMediaTime(&hls_seek_time_);
    hls_seek_time_ = hls_seek_time_ + file_info.start_time;
    CORE_PLAYER_RUNTIME("HLS last seek time is "TIME_FORMAT"(%lld us)",
        TIME_ARGS(hls_seek_time_), hls_seek_time_);
  }

  if (state_ > STATE_IDLE) {
    ret = mRawDataReader_->Start(1.0);
    if (kSuccess != ret) {
      CORE_PLAYER_ERROR("Failed to start data reader!");
      return ret;
    }
  } else {
    if (cb_target_) {
      cb_target_->sendMsg(MSG_SEEK_COMPLETE);
    }
  }

  for (vector<TimedTextDemux *>::iterator it = ext_srt_dmx_list_.begin();
    it != ext_srt_dmx_list_.end(); ++it) {
    (*it)->start();
  }

  CORE_PLAYER_RUNTIME("After seek, data reader has resumed and begins reading data.");

  CORE_PLAYER_RUNTIME("EXIT");
  return kSuccess;
}

MediaResult CorePlayer::setVolume(float leftVolume, float rightVolume) {
  if (mMHALAudioRenderer_) {
    mMHALAudioRenderer_->setVolume(leftVolume, rightVolume);
  }
  return kSuccess;
}

// TODO: we need to consider external subtitles
MediaResult CorePlayer::SelectSubtitleStream(int32_t sub_idx) {
  CORE_PLAYER_API("ENTER");

  if (sub_idx >= GetSubtitleStreamCount() || sub_idx < 0) {
    CORE_PLAYER_API("Invalid subtitle stream id(%d)", sub_idx);
    return kUnexpectedError;
  }

  if (sub_idx == subtitle_id_) {
    return kSuccess;
  }

  sending_subtitle_ = true;

  MmpElement* engine = NULL;

  if (subtitle_id_ != -1) {
    MmpElement* engine = subtitle_engines_[subtitle_id_];
    M_ASSERT_FATAL(engine, kUnexpectedError);

    // Deactive old subtitle
    MmpEvent deact_event(MmpEvent::MMP_EVENT_DEACTIVATE);
    engine->SendEvent(&deact_event);
  }
  // Send active event to new subtitle engine
  subtitle_id_ = sub_idx;
  engine = subtitle_engines_[subtitle_id_];
  M_ASSERT_FATAL(engine, kUnexpectedError);
  MmpEvent act_event(MmpEvent::MMP_EVENT_ACTIVATE);
  engine->SendEvent(&act_event);

  CORE_PLAYER_API("EXIT");

  return kSuccess;
}

MediaResult CorePlayer::DeselectSubtitleStream(int32_t sub_idx) {
  if (sub_idx >= GetSubtitleStreamCount() || sub_idx < 0) {
    CORE_PLAYER_API("Invalid subtitle stream id(%d)", sub_idx);
    return kUnexpectedError;
  }

  if (subtitle_id_ != sub_idx) {
    return kUnexpectedError;
  }

  sending_subtitle_ = false;

  MmpElement* engine = subtitle_engines_[subtitle_id_];
  MmpEvent deact_event(MmpEvent::MMP_EVENT_DEACTIVATE);
  engine->SendEvent(&deact_event);

  subtitle_id_ = -1;
  return kSuccess;
}


MediaResult CorePlayer::SelectAudioStream(int32_t audio_idx) {
  CORE_PLAYER_API("ENTER");

  // If no audio or just 1 audio.
  if (file_info.supported_audio_counts <= 1 ) {
    CORE_PLAYER_ERROR("No audio streams available!");
    return kSuccess;
  }

  if (audio_idx >= file_info.supported_audio_counts || audio_idx < 0) {
    CORE_PLAYER_ERROR("Audio stream id invalid!");
    return kUnexpectedError;
  }

  if (audio_id_ == audio_idx) {
    CORE_PLAYER_ERROR("We are playing this audio stream now!");
    return kSuccess;
  }

  // connect the demux & omx comp
  MmpPad* audio_pad = mRawDataReader_->GetSrcPadByPrefix("audio", audio_idx);
  if (!audio_pad) {
    CORE_PLAYER_ERROR("No pad for stream %d found!", audio_idx);
    return kSuccess;
  }
  const char* mime_type = NULL;
  MmpCaps &caps = audio_pad->GetCaps();
  caps.GetString("mime_type", &mime_type);
  if (!MHALAudioDecoder::isAudioSupported(mime_type)) {
    CORE_PLAYER_ERROR("The audio is not supported!");
    return kUnexpectedError;
  }

  if (!mMHALAudioRenderer_) {
    CORE_PLAYER_ERROR("We are not playing audio!");
    return kUnexpectedError;
  }

  CORE_PLAYER_RUNTIME("Will change to play audio stream id %d:", audio_idx);

  // Stop audio and tell data reader the stream index.
  current_audio_ = -1;
  mRawDataReader_->SetCurrentStream(current_video_, current_audio_);

  // Delete the old audio decoder firstly.
  if (mMHALAudioDecoder_) {
    // Audio decoder stop sending data.
    mMHALAudioDecoder_->canSendBufferToOmx(OMX_FALSE);

    mFlushCount_ = 2;
    mFlushSignal_->resetSignal();
    mMHALAudioDecoder_->flushAsync();
    mMHALAudioRenderer_->flushAsync();
    mFlushSignal_->waitSignal();

    // Put audio pipeline into idle.
    mMHALAudioDecoder_->setOmxState(OMX_StateIdle);
    mMHALAudioRenderer_->setOmxState(OMX_StateIdle);

    // Put audio pipeline into loaded.
    mMHALAudioDecoder_->setOmxState(OMX_StateLoaded);
    mMHALAudioRenderer_->setOmxState(OMX_StateLoaded);

    // Free old audio decoder.
    vector<MHALOmxComponent *>::iterator it;
    for (it = mMHALOmxComponents_.begin(); it != mMHALOmxComponents_.end(); ++it) {
      MHALOmxComponent *comp = *(it);
      if (comp && (comp == mMHALAudioDecoder_)) {
        mMHALOmxComponents_.erase(it);
        CORE_PLAYER_RUNTIME("Found audio decoder and destroy it.");
        delete mMHALAudioDecoder_;
        mMHALAudioDecoder_ = NULL;
        break;
      }
    }
  }

  //  Create audio decoder for the new audio substream.
  mMHALAudioDecoder_ = new MHALAudioDecoder(this);
  // connect the demux & omx comp
  MmpPad* audio_src_pad = mRawDataReader_->GetSrcPadByPrefix("audio", audio_idx);
  MmpPad* audio_sink_pad = mMHALAudioDecoder_->GetSinkPad();
  if (audio_src_pad && audio_sink_pad) {
    MmpPad::Link(audio_src_pad, audio_sink_pad);
  }
  if (!mMHALAudioDecoder_->setUp()) {
    CORE_PLAYER_ERROR("The audio stream cannot be played!!!");
    delete mMHALAudioDecoder_;
    mMHALAudioDecoder_ = NULL;
    return kUnexpectedError;
  }
  mMHALOmxComponents_.push_back(mMHALAudioDecoder_);

  // Set up tunnel between audio decoder's output port and audio renderer's input port.
  mMHALAudioDecoder_->setupTunnel(kAudioPortStartNumber + 1,
      mMHALAudioRenderer_->getCompHandle(), kAudioPortStartNumber);

  // Put audio pipeline into idle.
  mMHALAudioDecoder_->setOmxState(OMX_StateIdle);
  mMHALAudioRenderer_->setOmxState(OMX_StateIdle);

  // Put audio pipeline into executing.
  mMHALAudioDecoder_->setOmxState(OMX_StateExecuting);
  mMHALAudioRenderer_->setOmxState(OMX_StateExecuting);

  // Update new audio ID and stream index to let DR start feeding data.
  audio_id_ = audio_idx;
  current_audio_ = file_info.audio_stream_info[audio_idx].stream_index;
  CORE_PLAYER_RUNTIME("Changed to play audio stream id %d, stream index %d.",
      audio_idx, current_audio_);

  // Tell data reader the stream index.
  mRawDataReader_->SetCurrentStream(current_video_, current_audio_);

  // Start Playback from new positon.
  TimeStamp switch_position = GetPosition();
  SetPosition(switch_position);

  // Audio decoder resume sending data.
  mMHALAudioDecoder_->pushCodecConfigData();
  mMHALAudioDecoder_->canSendBufferToOmx(OMX_TRUE);

  CORE_PLAYER_API("EXIT");
  return kSuccess;
}

bool CorePlayer::getAudioStreamsDescription(streamDescriptorList *desc_list) {
  uint32_t i = 0, j = 0;
  M_ASSERT_FATAL(desc_list, false);

  for (i = 0; i < GetAudioStreamCount(); i++) {
    const mchar* mime_type = NULL;
    const mchar* language = NULL;
    const mchar* description = NULL;
    mint32 codec_id;

    MmpPad* pad = mRawDataReader_->GetSrcPadByPrefix("audio", i);
    M_ASSERT_FATAL(pad, false);

    MmpCaps &caps = pad->GetCaps();

    caps.GetString("mime_type", &mime_type);
    caps.GetString("language", &language);
    caps.GetString("description", &description);

    streamDescriptor *sd = new streamDescriptor(mime_type, language, description);

    if (sd) {
      desc_list->pushItem(sd);
    }

  }
  return true;
}

bool CorePlayer::getSubtitleStreamsDescription(streamDescriptorList *desc_list) {
  uint32_t i = 0, j = 0;

  M_ASSERT_FATAL(desc_list, false);

  for (i = 0; i < GetInternalSubtitleStreamCount(); i++) {
    const mchar* mime_type = NULL;
    const mchar* language = NULL;
    const mchar* description = NULL;
    mint32 codec_id;

    MmpPad* pad = mRawDataReader_->GetSrcPadByPrefix("subtitle", i);
    M_ASSERT_FATAL(pad, false);

    MmpCaps &caps = pad->GetCaps();

    caps.GetString("mime_type", &mime_type);
    caps.GetString("language", &language);
    caps.GetString("description", &description);

    streamDescriptor *sd = new streamDescriptor(mime_type, language, description);

    if (sd) {
      desc_list->pushItem(sd);
    }

  }

  for (i = 0; i < GetExternalSubtitleStreamCount(); i++) {
    streamDescriptor *sd = new streamDescriptor("text/plain", "und", "");
    desc_list->pushItem(sd);
  }

  return true;
}

MediaResult CorePlayer::stopDataSource() {
  if (mRawDataSource_) {
    // We need this so that in case of Internet streaming, operation won't block.
    mRawDataSource_->stopDataSource();
    return kSuccess;
  }

  return kUnexpectedError;
}

bool CorePlayer::isLiveDataSource(){
  bool isLive = false;
  TimeStamp duration_ns = GetDuration();
  if (mRawDataSource_ && mRawDataSource_->isSeekable()) {
    if (duration_ns <= 0) {
      isLive = true;
    }
  }

  return isLive;
}

bool CorePlayer::createDataSource() {
  CORE_PLAYER_API("ENTER");
  if (url_) {
    CORE_PLAYER_RUNTIME("url is %s", url_);
#ifdef ENABLE_HLS
    mRawDataSource_ = new HttpLiveDataSource(url_);

    if (mRawDataSource_ && mRawDataSource_->isUrlHLS(url_)) {
      // Firstly check whether it is a HLS file.
      CORE_PLAYER_RUNTIME("Use Http Live Streaming Data Source");
      if (!mRawDataSource_->startDataSource()) {
        if (cb_target_) {
          int errorno = MRVL_PLAYER_ERROR_PLAYER_ERROR;
          cb_target_->sendMsg(MSG_ERROR, &errorno);
        }
        return kUnexpectedError;
      }
      data_source_type_ = SOURCE_HLS;
    }
    else
#endif
    if (HttpDataSource::isUrlHttp(url_)) {
      // Secondly check whether it is a http progressive stream.
      CORE_PLAYER_RUNTIME("Use HTTP File Data Source");
      delete mRawDataSource_;
      mRawDataSource_ = NULL;
      mRawDataSource_ = new HttpDataSource(url_);
      data_source_type_ = SOURCE_HTTP;
    } else {
      // Third check whether it is a local file.
      CORE_PLAYER_RUNTIME("Use Local File Data Source");
      delete mRawDataSource_;
      mRawDataSource_ = NULL;
      bool ret = false;
      mRawDataSource_ = new FileDataSource(url_, &ret);
      if (ret == true) {
        data_source_type_ = SOURCE_FILE;
      } else {
        delete mRawDataSource_;
        mRawDataSource_ = NULL;
        data_source_type_ = SOURCE_INVALID;
      }
    }

    if (mRawDataSource_) {
      mRawDataSource_->registerCallback(this);
    }
  } else if (source_fd_ >= 0) {
    CORE_PLAYER_RUNTIME("fd = %d, offset = %lld, length = %lld",
        source_fd_, source_offset_, source_length_);
    mRawDataSource_ = new FileDataSource(source_fd_, source_offset_, source_length_);
    data_source_type_ = SOURCE_FILE;
    if (mRawDataSource_) {
      mRawDataSource_->registerCallback(this);
    }
  }

  CORE_PLAYER_API("EXIT");
  return true;
}

MediaResult CorePlayer::SetSource(const char* url, const KeyedVector<String8, String8> *headers) {
  CORE_PLAYER_API("ENTER");

  Stop();

  if (url_) {
    free(url_);
    url_ = NULL;
  }

  url_ = (char *)malloc(strlen((const char*)url)+1);
  if (url_) {
    memset(url_, 0x0, strlen((const char*)url)+1);
  } else {
    return kOutOfMemory;
  }
  if (!strncmp((const char*)url,"file://",7)) {
    strcpy(url_, (const char*)(&url[7]));
  } else if(strncmp(url, "iptv", 4) == 0) {
    strcpy(url_, (const char*)(&url[4]));
  } else {
    strcpy(url_, (const char*)url);
  }

  CORE_PLAYER_API("EXIT");
  return kSuccess;
}

MediaResult CorePlayer::SetSource(int fd, uint64_t offset, uint64_t length) {
  CORE_PLAYER_API("ENTER");
  Stop();
  if (url_) {
    free(url_);
    url_ = NULL;
  }

  source_fd_ = dup(fd);
  source_offset_ = offset;
  source_length_ = length;

  // This hack is to pass CTS test.
  if (probeStreamInfo() != kSuccess) {
    return kUnexpectedError;
  }

  CORE_PLAYER_API("EXIT");
  return kSuccess;
}

MediaResult CorePlayer::SetSecondSource(const char* url, const char* mime_type) {
  //TODO: need support this later
  ext_sub_count_ ++;
  return kSuccess;
}


MediaResult CorePlayer::SetSecondSource(int32_t fd, uint64_t offset, uint64_t length,
                                        const char* mime_type) {
  ext_sub_count_ ++;
  int dup_fd = dup(fd);

  TimedTextDemux* ext_srt_demux = new TimedTextDemux(dup_fd, offset, length);
  ext_srt_dmx_list_.push_back(ext_srt_demux);

  return kSuccess;
}

bool CorePlayer::isDataBuffered() {
  const TimeStamp BUFFERED_THRESHOLD = 1 * 1000 * 1000; // 1s

  TimeStamp last_pts = mRawDataReader_->getLatestPts();
  bool data_eos = mRawDataReader_->isDataReaderEOS();

  if (data_eos || (kInvalidTimeStamp == last_pts)) {
    return true;
  }

  if (last_pts - buffering_start_pts_ >= BUFFERED_THRESHOLD) {
    return true;
  }

  return false;
}


MmpError CorePlayer::buildVideoPipeline() {
  mMHALVideoDecoder_ = new MHALVideoDecoder(this);
  if (mMHALVideoDecoder_) {
    mMHALVideoDecoder_->setNativeWindow(mNativeWindow_);
    // connect the demux & omx comp
    MmpPad* video_src_pad = mRawDataReader_->GetSrcPadByPrefix("video", 0);
    MmpPad* video_sink_pad = mMHALVideoDecoder_->GetSinkPad();
    if (video_src_pad && video_sink_pad) {
      MmpPad::Link(video_src_pad, video_sink_pad);
    }
  }
  if (mMHALVideoDecoder_ && mMHALVideoDecoder_->setUp(omx_tunnel_mode_)) {
    video_id_ = 0;
    current_video_ = file_info.video_stream_info[0].stream_index;
    mMHALOmxComponents_.push_back(mMHALVideoDecoder_);
    CORE_PLAYER_RUNTIME("current_video_ = %d", current_video_);

    if (OMX_TUNNEL == omx_tunnel_mode_) {
      CORE_PLAYER_RUNTIME("Build video pipeline in tunnel mode.");
      MHALOmxComponent *video_processor = new MHALVideoProcessor(this);
      video_processor->setUp();

      MHALOmxComponent *video_scheduler = new MHALVideoScheduler(this);
      video_scheduler->setUp();

      MHALOmxComponent *video_renderer = new MHALVideoRenderer(this);
      video_renderer->setUp();

      // Set up tunnel between video decoder's output port and video processor's input port.
      mMHALVideoDecoder_->setupTunnel(kVideoPortStartNumber + 1,
          video_processor->getCompHandle(), kVideoPortStartNumber);

      // Set up tunnel between video processor's output port and video scheduler's input port.
      video_processor->setupTunnel(kVideoPortStartNumber + 1,
          video_scheduler->getCompHandle(), kVideoPortStartNumber);

      // Set up tunnel between video processor's output port and video scheduler's input port.
      video_scheduler->setupTunnel(kVideoPortStartNumber + 1,
          video_renderer->getCompHandle(), kVideoPortStartNumber);

      // Set up tunnel between clock's scond port and audio renderer's clock port.
      mMHALClock_->setupTunnel(kClockPortStartNumber + 1,
          video_scheduler->getCompHandle(), kClockPortStartNumber);

      mMHALOmxComponents_.push_back(video_processor);
      mMHALOmxComponents_.push_back(video_scheduler);
      mMHALOmxComponents_.push_back(video_renderer);
    } else if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
      CORE_PLAYER_RUNTIME("Build video pipeline in non-tunnel mode.");
      mAndroidNativeWindowRenderer_ = new AndroidNativeWindowRenderer(mNativeWindow_);
      // Connect the omx decoder -> render data path.
      MmpPad* src_pad = mMHALVideoDecoder_->GetSrcPadByPrefix("video_src", 0);
      MmpPad* sink_pad = mAndroidNativeWindowRenderer_->GetSinkPad();
      if (src_pad && sink_pad) {
        MmpPad::Link(src_pad, sink_pad);
      }

      mAndroidNativeWindowRenderer_->SetClock(mMmpClock_);
    }
  } else {
    CORE_PLAYER_ERROR("failed to set up video decoder!!!");
    delete mMHALVideoDecoder_;
    mMHALVideoDecoder_ = NULL;
  }

  return MMP_NO_ERROR;
}

MmpError CorePlayer::buildAudioPipeline() {
  mMHALAudioDecoder_ = new MHALAudioDecoder(this);
  M_ASSERT_FATAL(mMHALAudioDecoder_, MMP_UNEXPECTED_ERROR);

  // Loop all audio stream until find one to play.
  uint32_t idx = 0;
  for (idx = 0; idx < file_info.supported_audio_counts; idx++) {
    // connect the demux & omx comp
    MmpPad* audio_src_pad = mRawDataReader_->GetSrcPadByPrefix("audio", idx);
    MmpPad* audio_sink_pad = mMHALAudioDecoder_->GetSinkPad();
    if (audio_src_pad && audio_sink_pad) {
      MmpPad::Link(audio_src_pad, audio_sink_pad);
    }
    if (mMHALAudioDecoder_->setUp(omx_tunnel_mode_)) {
      audio_id_ = idx;
      current_audio_ = file_info.audio_stream_info[idx].stream_index;
      mMHALOmxComponents_.push_back(mMHALAudioDecoder_);
      CORE_PLAYER_RUNTIME("current_audio_ = %d", current_audio_);
    } else {
      continue;
    }
    break;
  }
  if (idx < file_info.supported_audio_counts) {
    if (OMX_TUNNEL == omx_tunnel_mode_) {
      CORE_PLAYER_RUNTIME("Build audio pipeline in tunnel mode.");
      mMHALAudioRenderer_ = new MHALAudioRenderer(this);
      mMHALAudioRenderer_->setUp();

      // Set up tunnel between audio decoder's output port and audio renderer's input port.
      mMHALAudioDecoder_->setupTunnel(kAudioPortStartNumber + 1,
          mMHALAudioRenderer_->getCompHandle(), kAudioPortStartNumber);

      if (hasVideo()) {
        // Set up tunnel between clock's first port and audio renderer's clock port.
        mMHALClock_->setupTunnel(kClockPortStartNumber,
            mMHALAudioRenderer_->getCompHandle(), kClockPortStartNumber);
      } else {
        // For audio only stream, disable A/V sync.
        mMHALAudioRenderer_->disablePort(kClockPortStartNumber);
      }
      mMHALOmxComponents_.push_back(mMHALAudioRenderer_);
    } else if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
      CORE_PLAYER_RUNTIME("Build audio pipeline in non-tunnel mode.");
      mAndroidAudioRenderer_ = new AndroidAudioRenderer();
      // Connect the omx decoder -> render data path.
      MmpPad* src_pad = mMHALAudioDecoder_->GetSrcPadByPrefix("audio_src", 0);
      MmpPad* sink_pad = mAndroidAudioRenderer_->GetSinkPad();
      if (src_pad && sink_pad) {
        MmpPad::Link(src_pad, sink_pad);
      }
    }
  } else {
    CORE_PLAYER_ERROR("No audio stream can be played!!!");
    delete mMHALAudioDecoder_;
    mMHALAudioDecoder_ = NULL;
  }
  return MMP_NO_ERROR;
}


MmpError CorePlayer::buildSubtitlePipeline() {
  for (muint32 i = 0; i < GetInternalSubtitleStreamCount(); i++) {
    MmpElement* engine = new MHALSubtitleEngine(this);
    engine->SetClock(mMmpClock_);
    subtitle_id_ = 0;

    // connect the demux & subtitle engine
    MmpPad* sub_src_pad = mRawDataReader_->GetSrcPadByPrefix("subtitle", i);
    MmpPad* sub_sink_pad = engine->GetSinkPad();

    MmpPad::Link(sub_src_pad, sub_sink_pad);

    subtitle_engines_.push_back(engine);
  }

  for(muint32 i = 0; i < GetExternalSubtitleStreamCount(); i++) {
    MmpElement* engine = new MHALSubtitleEngine(this);
    engine->SetClock(mMmpClock_);

    // connect the demux & subtitle engine
    MmpPad* sub_src_pad = ext_srt_dmx_list_[i]->GetSrcPadByPrefix("ext", 0);
    MmpPad* sub_sink_pad = engine->GetSinkPad();

    MmpPad::Link(sub_src_pad, sub_sink_pad);

    subtitle_engines_.push_back(engine);
  }

  MmpElement* engine = subtitle_engines_[0];

  MmpEvent event(MmpEvent::MMP_EVENT_ACTIVATE);
  engine->SendEvent(&event);

  return MMP_NO_ERROR;
}

// Return true when underrun happens, and should pause clock component in order to buffer data.
// Return false when data buffered, and should un-pause clock component to resume playback.
// If (the last packet's pts <= current position + underrun threshold),
//     underrun happens.
// If (the last packet's pts >= current position + buffered threshold),
//     data buffered, resume playback.
bool CorePlayer::isUnderrun() {
  const TimeStamp UNDERRUN_THRESHOLD = 500 * 1000; // 0.5s

  const TimeStamp LOCAL_FILE_BUFFERED_THRESHOLD = 1 * 1000 * 1000; // 1s

  // TODO: threshold should be tuned well. Maybe bytes also need be considered.
  const TimeStamp INTERNET_BUFFERED_THRESHOLD = 5 * 1000 * 1000; // 5s

  const int kBufferHighWatermark = 90;

  if (state_ <= STATE_IDLE) {
    // If playback is not start yet, don't detect underrun.
    return false;
  }

  // Because this function gets called every 200ms, so to drive sutitle engine to
  // show subtitle according to media time.
  if (subtitle_id_ != -1) {
    MmpEvent update_event(MmpEvent::MMP_EVENT_UPDATE);
    subtitle_engines_[subtitle_id_]->SendEvent(&update_event);
  }

  // There are two kinds of buffering:
  // 1. buffering after first time play or after seek,
  // 2. buffering after underrun.
  if (b_pending_start_) {
    // If it is during the buffering after first time play or after seek,
    // player needs buffering 1s before start playback.
    if (isDataBuffered()) {
      b_pending_start_ = false;
      b_underrun_status_ = false; // It is not underrun at this moment.
      if (mMmpClock_ && (STATE_PLAYING == state_)) {
        CORE_PLAYER_RUNTIME("Set clock scale to 1.0 to resume playback.");
        mMmpClock_->setScale(1.0);
      }
      if (cb_target_) {
        int buffering_level = 100;
        cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
      }
      // Data is buffered for first time play or after seek, resume playback.
      return false;
    }
  } else {
    // If it is during the buffering after underrun:
    TimeStamp playtime = kInvalidTimeStamp;
    TimeStamp buffered_threshold = 0;
    TimeStamp duration = 0, difftime = 0;
    int audio_buf_level = 0;
    int video_buf_level = 0;
    TimeStamp last_pts = mRawDataReader_->getLatestPts();
    bool data_eos = mRawDataReader_->isDataReaderEOS();

    if (mMmpClock_) {
      playtime = mMmpClock_->GetTime();
    }

    if (SOURCE_FILE == getDataSourceType()) {
      buffered_threshold = LOCAL_FILE_BUFFERED_THRESHOLD;
    } else if ((SOURCE_HTTP == getDataSourceType()) || (SOURCE_HLS == getDataSourceType())) {
      duration = GetDuration();
      difftime = (duration + file_info.start_time - playtime);
      if (difftime > INTERNET_BUFFERED_THRESHOLD) {
        buffered_threshold = INTERNET_BUFFERED_THRESHOLD;
      } else {
        buffered_threshold = difftime / 2;
      }
    }

    // Firstly check pts to see whether underrun.
    if (last_pts >= playtime + buffered_threshold) {
      b_is_underrun_ = false;
    }
    if (last_pts <= playtime + UNDERRUN_THRESHOLD) {
      b_is_underrun_ = true;
    }

    // Secondly check buffering level to see whether enough data is buffered.
    if (hasAudio() && (mMHALAudioDecoder_ != NULL)) {
      audio_buf_level = mMHALAudioDecoder_->getBufferLevel();
    }
    if (hasVideo() && (mMHALVideoDecoder_ != NULL)) {
      video_buf_level = mMHALVideoDecoder_->getBufferLevel();
    }

    // For high bitrate stream, the buffer queue may be full before the desired
    // "buffered_threshold" can be reached.
    if ((audio_buf_level >= kBufferHighWatermark) ||
        (video_buf_level >= kBufferHighWatermark)) {
      b_is_underrun_ = false;
    }

    // Thirdly if pts is not reliable or EOS is reached, let it play.
    if (data_eos || (kInvalidTimeStamp == last_pts)) {
      b_is_underrun_ = false;
    }

    if (b_is_underrun_ && !b_underrun_status_) {
      CORE_PLAYER_RUNTIME("Underrun happens, pause playback and start buffering data.");
      b_underrun_status_ = true;
      if (mMmpClock_) {
        mMmpClock_->setScale(0.0);
      }
      if (cb_target_) {
        int buffering_level = 0;
        cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
      }
    } else if (!b_is_underrun_ && b_underrun_status_) {
      CORE_PLAYER_RUNTIME("Enough data is buffered.");
      b_underrun_status_ = false;
      if (STATE_PLAYING == state_) {
        CORE_PLAYER_RUNTIME("Resume playback.");
        if (mMmpClock_) {
          mMmpClock_->setScale(1.0);
        }
      }
      if (cb_target_) {
        int buffering_level = 100;
        cb_target_->sendMsg(MSG_BUFFERING_UPDATE , &buffering_level);
      }
    }
  }

  return b_underrun_status_;
}

#ifdef ENABLE_RESOURCE_MANAGER
MediaResult CorePlayer::RequestResource() {
  CORE_PLAYER_API("ENTER");
  int ret = -1;

  if (mResourceReadySignal_) {
    mResourceReadySignal_->resetSignal();
  }

  mMrvlMediaPlayerClient_ = new ResourceClient();
  if (mMrvlMediaPlayerClient_ == NULL) {
    CORE_PLAYER_ERROR("Failed to create resouce manager client.");
    return kUnexpectedError;
  }

  mMrvlMediaPlayerClient_->setApplicationName(String8("MrvlMediaPlayer"));
  if (!mMrvlMediaPlayerClient_->registerEventHandler(this)) {
    CORE_PLAYER_ERROR("Failed to registerEventHandler");
    return kUnexpectedError;
  }
  getResourceManager()->RegisterClient(mMrvlMediaPlayerClient_, &mResourceClientHandle_);

  if (hasVideo()) {
    sp<ResourceDesc> video_channel_desc = new ResourceDesc;
    video_channel_desc->setResourceType(String8("VPP"));
    video_channel_desc->addCaps(String8("de-interlace"));
    ret = getResourceManager()->ReserveResourceAsync(mResourceClientHandle_, video_channel_desc);
    if (ret != 0) {
      CORE_PLAYER_ERROR("request VPP failed");
      return kUnexpectedError;
    }

    sp<ResourceDesc> video_decoder_desc = new ResourceDesc;
    video_decoder_desc->setResourceType(String8("Video Codec"));
    video_decoder_desc->addCaps(String8("Decode"));
    video_decoder_desc->addCaps(String8("HD"));
    // TODO: we have to tell ResourceManager codec type
    //video_decoder_desc->addCaps(String8("H.264"));
    ret = getResourceManager()->ReserveResourceAsync(mResourceClientHandle_, video_decoder_desc);
    if (ret != 0) {
      CORE_PLAYER_ERROR("request Video Codec failed");
      return kUnexpectedError;
    }
  }

  if (hasAudio()) {
    sp<ResourceDesc> audio_channel_desc = new ResourceDesc;
    audio_channel_desc->setResourceType(String8("APP"));
    ret = getResourceManager()->ReserveResourceAsync(mResourceClientHandle_, audio_channel_desc);
    if (ret != 0) {
      CORE_PLAYER_ERROR("request APP failed");
      return kUnexpectedError;
    }

    sp<ResourceDesc> audio_decoder_desc = new ResourceDesc;
    audio_decoder_desc->setResourceType(String8("Audio Codec"));
    ret = getResourceManager()->ReserveResourceAsync(mResourceClientHandle_, audio_decoder_desc);
    if (ret != 0) {
      CORE_PLAYER_ERROR("request Audio Codec failed");
      return kUnexpectedError;
    }
  }

  if (mResourceReadySignal_) {
    CORE_PLAYER_RUNTIME("waiting for resource");
    mResourceReadySignal_->waitSignal();
  }

  CORE_PLAYER_API("EXIT");
  return kSuccess;
}

MediaResult CorePlayer::ReleaseResource() {
  CORE_PLAYER_API("ENTER");

  if (app_resource_handle_ > 0) {
    getResourceManager()->ReleaseResource(app_resource_handle_);
    app_resource_handle_ = 0;
    audio_channel_ = -1;
    CORE_PLAYER_RUNTIME("APP has been released to ResourceManager");
  }
  if (audio_decoder_handle_ > 0) {
    getResourceManager()->ReleaseResource(audio_decoder_handle_);
    audio_decoder_handle_ = 0;
    CORE_PLAYER_RUNTIME("audio decoder has been released to ResourceManager");
  }
  if (vpp_resource_handle_ > 0) {
    getResourceManager()->ReleaseResource(vpp_resource_handle_);
    vpp_resource_handle_ = 0;
    video_channel_ = -1;
    CORE_PLAYER_RUNTIME("VPP has been released to ResourceManager");
  }
  if (video_decoder_handle_ > 0) {
    getResourceManager()->ReleaseResource(video_decoder_handle_);
    video_decoder_handle_ = 0;
    CORE_PLAYER_RUNTIME("video decoder has been released to ResourceManager");
  }
  if (mMrvlMediaPlayerClient_ != NULL) {
    mMrvlMediaPlayerClient_->unregisterEventHandler(this);
    mMrvlMediaPlayerClient_ = NULL;
  }
  if (mResourceClientHandle_ > 0) {
    getResourceManager()->UnregisterClient(mResourceClientHandle_);
    mResourceClientHandle_ = 0;
    CORE_PLAYER_RUNTIME("ResourceManager client has been released");
  }

  if (mResourceReadySignal_) {
    delete mResourceReadySignal_;
    mResourceReadySignal_ = NULL;
  }

  CORE_PLAYER_API("EXIT");
  return kSuccess;
}

bool CorePlayer::gotAllResource() {
  bool got_audio_resource = false;
  bool got_video_resource = false;

  if (hasAudio()) {
    if ((app_resource_handle_ > 0) && (audio_decoder_handle_ > 0)) {
      got_audio_resource = true;
    }
  } else {
    got_audio_resource = true;
  }

  if (hasVideo()) {
    if ((vpp_resource_handle_ > 0) && (video_decoder_handle_ > 0)) {
      got_video_resource = true;
    }
  } else {
    got_video_resource = true;
  }

  return got_audio_resource & got_video_resource;
}

void CorePlayer::handleRMEvent(RMEvent event, ResHandle handle, sp<ResourceDesc>& desc) {
  uint32_t channel_info = 0;
  int errorno = MRVL_PLAYER_ERROR_REQUEST_RESOURCE_FAIL;

  if (RMEvent_ReserveDone == event) {
    const char* resourceType = desc->getResourceType().string();
    if (strncmp(resourceType, "VPP", 3) == 0) {
      vpp_resource_handle_ = handle;
      video_channel_ = ResourceUtility::Desc2VideoChannel(desc);
      CORE_PLAYER_RUNTIME("Has got VPP from resoure manager, video_channel_ = %d",
          video_channel_);
    } else if (strncmp(resourceType, "Video Codec", 11) == 0) {
      video_decoder_handle_ = handle;
      CORE_PLAYER_RUNTIME("Has got VCODEC from resoure manager");
    } else if (strncmp(resourceType, "APP", 3) == 0) {
      app_resource_handle_ = handle;
      audio_channel_ = ResourceUtility::Desc2AudioChannel(desc);
      CORE_PLAYER_RUNTIME("Has got APP from resoure manager, audio_channel_ = %d",
          audio_channel_);
    } else if (strncmp(resourceType, "Audio Codec", 11) == 0) {
      audio_decoder_handle_ = handle;
      CORE_PLAYER_RUNTIME("Has got ACODEC from resource manager.");
    } else {
        CORE_PLAYER_ERROR("Unexpected resource allocation (%s)", resourceType);
    }

    if (gotAllResource()) {
      CORE_PLAYER_RUNTIME("Got all resource from resource manager.");
      if (mResourceReadySignal_) {
        mResourceReadySignal_->setSignal();
      }
      if (cb_target_) {
        channel_info = (audio_channel_ << 16) | (video_channel_);
        cb_target_->sendMsg(MSG_RESOURCE_READY, &channel_info);
      }
    }
  } else if (RMEvent_Stealing == event || RMEvent_AboutToKill == event) {
    CORE_PLAYER_RUNTIME("send MEDIA_ERROR");
    if (mResourceReadySignal_) {
      mResourceReadySignal_->setSignal();
    }
    if (cb_target_) {
      cb_target_->sendMsg(MSG_ERROR, &errorno);
    }
  } else if (RMEvent_MasterChanged == event) {
    CORE_PLAYER_ERROR("Master changed, do nothing");
  } else {
    CORE_PLAYER_ERROR("Failed to get resources, event = %d", event);
    if (mResourceReadySignal_) {
      mResourceReadySignal_->setSignal();
    }
    if (cb_target_) {
      cb_target_->sendMsg(MSG_ERROR, &errorno);
    }
  }

  return;
}
#endif

}
