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

extern "C" {
#include <poll.h>
#include <pthread.h>
}

#include <utils/Log.h>
#include <media/mediaplayer.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>

#include "MediaPlayerOnlineDebug.h"
#include "MRVLFFPlayer.h"

#undef  LOG_TAG
#define LOG_TAG "MRVLFFPlayer"

namespace mmp {

#define MSToUs 1000

// 10M is a proper buffer size. It won't take too long to buffer 10M, also buffering won't
// happen too frequently.
#define VOD_Buffering_Threshold (10 * 1000 * 1000)
#define Live_BUffering_Threshold (1 * 1000 * 1000)

volatile int32_t MRVLFFPlayer::instance_id_ = 0;

MRVLFFPlayer::MRVLFFPlayer(MRVLFFPlayerCbTarget *target)
    : cur_play_speed_(1.0),
      work_thread_id_(0),
      pending_seek_(false),
      pending_seek_target_(0),
      seek_in_progress_(false),
      cb_target_(target),
      is_eos_sent_(false) {
  // initilize members
  front_ = 0;
  end_ = 0;
  for (int i = 0; i < QUEUE_LEN; i++) {
    state_queue_[i] = State_Invalid;
  }

  cur_state_ = State_WaitingForPrepare;
  tgt_state_ = State_Invalid;

  is_loopback_enabled_ = false;

  sprintf(log_tag_, "MRVLFFPlayer%d", instance_id_);
  android_atomic_inc(&instance_id_);

  core_player_ = new CorePlayer(this);
  MV_LOGI("Create MRVLFFPlayer Done");
}

MRVLFFPlayer::~MRVLFFPlayer() {
  MV_LOGI("Begin to destroy FFMPEG Player");
  reset();
  if(core_player_) {
    core_player_->Reset();
    delete core_player_;
    core_player_ = NULL;
  }
  MV_LOGI("Destroy FFMPEG Player Done");
}

status_t MRVLFFPlayer::sendMsg(MSG_TYPE message, void* data) {
  uint32_t width, height, resolution;
  uint32_t channel_info;
  int *errorno = NULL;
  int progress = 0;
  char *ip_addr = NULL;
  switch (message) {
    case MSG_EOS:
      if (is_loopback_enabled_) {
        MV_LOGD("EOS recieved and loop is set, play again.");
        seekTo(0);
      } else {
        MV_LOGD("EOS recieved, send MEDIA_PLAYBACK_COMPLETE to player.");
        if (!is_eos_sent_) {
          if (cb_target_) {
            MV_LOGD("send MEDIA_PLAYBACK_COMPLETE to application");
            cb_target_->sendMsg(MEDIA_PLAYBACK_COMPLETE);
          }
          is_eos_sent_ = true;
          // As android http://developer.android.com/reference/android/media/MediaPlayer.html
          // While in the PlaybackCompleted state, calling start() can restart the playback from
          // the beginning of the audio/video source.
          pause();
          seekTo(0);
        }
      }
      break;

    case MSG_RES_CHANGE:
      resolution = *(reinterpret_cast<uint32_t*>(data));
      width = resolution >> 16;
      height = resolution & 0xFFFF;
      MV_LOGD("Got resolution width = %d, height = %d.", width, height);
      if (cb_target_) {
        cb_target_->sendMsg(MEDIA_SET_VIDEO_SIZE, width, height);
      }
      break;

    case MSG_RENDERING_START:
      if (cb_target_) {
        cb_target_->sendMsg(MEDIA_INFO, MEDIA_INFO_RENDERING_START, 0);
      }
      break;

    case MSG_ERROR:
      MV_LOGD("---ERROR MSG---");
      if ((State_Preparing == cur_state_) && (State_Stopped == tgt_state_)) {
        // error is reported during prepare.
      } else {
        // On error, shut down the pipeline.
        // Call reset() may result in deadlock, we'd better do it asynchronously.
        action_mutex_.lock();
        setNextState_l(State_Shutdown);
        wakeup_event_.setEvent();
        action_mutex_.unlock();
      }
      if (cb_target_) {
        errorno = (int *)data;
        cb_target_->sendMsg(MEDIA_ERROR, 100, *errorno);
      }
      break;

    case MSG_TIMED_TEXT:
      if (cb_target_) {
        cb_target_->sendMsg(MEDIA_TIMED_TEXT, 0, 0, data);
      }
      break;

    case MSG_SEEK_COMPLETE:
      MV_LOGD("---SEEK COMPLETE MSG---");
      if (cb_target_) {
        cb_target_->sendMsg(MEDIA_SEEK_COMPLETE);
      }
      seek_in_progress_ = false;
      break;

    case MSG_RESOURCE_READY:
      MV_LOGD("---RESOURCE READY MSG---");
      channel_info = *(reinterpret_cast<uint32_t*>(data));
      if (cb_target_) {
        cb_target_->sendMsg(MEDIA_INFO, MSG_RESOURCE_READY, channel_info);
      }
      break;

    case MSG_PLAYING_BANDWIDTH:
      MV_LOGD("---HLS PLAYING_BANDWIDTH MSG---");
      if (cb_target_) {
        int *ptr = (int *)data;
        int bandwidth = *ptr;
        MV_LOGD("new bandwidth for HLS is %d bps", bandwidth);
        cb_target_->sendMsg(MEDIA_INFO, MSG_PLAYING_BANDWIDTH, bandwidth);
      }
      break;

    case MSG_HLS_REALTIME_SPEED:
      MV_LOGD("---HLS REALTIME_SPEED MSG---");
      if (cb_target_) {
        int *ptr = (int *)data;
        int realtime_speed = (*ptr) / 1000;
        MV_LOGD("HLS TS real time download speed is %d Kbit/s", realtime_speed);
        cb_target_->sendMsg(MEDIA_INFO, MSG_HLS_REALTIME_SPEED, realtime_speed);
      }
      break;

    case MSG_HLS_IP_ADDRESS:
      ip_addr = (char *)data;
      if (cb_target_) {
        // Notify application that we get a new IP address, then app should get it through invoke().
        cb_target_->sendMsg(MEDIA_INFO, MSG_HLS_IP_ADDRESS, (int)data);
      }
      break;

    case MSG_BUFFERING_UPDATE:
      if (cb_target_) {
        int *buffering_level = (int *)data;
        MV_LOGD("Have buffered %d%% data", *buffering_level);
        cb_target_->sendMsg(MEDIA_BUFFERING_UPDATE, *buffering_level);
      }
      break;

    default:
      break;
  }
  return 1;
}

status_t MRVLFFPlayer::setDataSource(const char *url) {
  bool ret = false;

  MV_LOGI("setDataSource, url=%s", url);

  MmuAutoLock lock(action_mutex_);
  if (kSuccess != core_player_->SetSource(url)) {
    MV_LOGE("Failed to set source for FF player");
    return false;
  }

  MV_LOGI("LEAVE");
  return true;
}

status_t MRVLFFPlayer::setDataSource(const char *url,
  const KeyedVector<String8, String8> *headers) {

  bool ret = false;
  android::String8 cookie;
  cookie.append("COOKIE");
  android::String8 value;

  if (headers != NULL) {
    value.append(headers->valueAt(0));
  }

  MV_LOGI("setDataSource, url=%s, headers = %p", url, headers);

  MmuAutoLock lock(action_mutex_);
  if (kSuccess != core_player_->SetSource(url, headers)) {
    MV_LOGE("Failed to set source for FF player");
    return false;
  }

  MV_LOGI("LEAVE");
  return true;
}

status_t MRVLFFPlayer::setDataSource(int fd, int64_t offset, int64_t length) {
  bool ret = false;

  MV_LOGI("setDataSource, fd=%d, offset=%lld, length=%lld", fd, offset, length);

  MmuAutoLock lock(action_mutex_);

  if (kSuccess != core_player_->SetSource(fd, offset, length)) {
    MV_LOGE("Failed to set source for FF player");
    return false;
  }

  MV_LOGI("LEAVE");
  return true;
}

status_t MRVLFFPlayer::setSecondDataSource(const char* url, const char* mime_type) {
  MmuAutoLock lock(action_mutex_);
  MV_LOGI("setSecondDataSource, url=%s", url);

  if (kSuccess != core_player_->SetSecondSource(url, mime_type)) {
    MV_LOGE("Failed to set second source for FF player");
    return false;
  }
  MV_LOGI("LEAVE");
  return true;
}



status_t MRVLFFPlayer::setSecondDataSource(int fd, int64_t offset, int64_t length,
                                        const char* mime_type){

  bool ret = false;

  MV_LOGI("setSecondDataSource, fd=%d, offset=%lld, length=%lld", fd, offset, length);

  MmuAutoLock lock(action_mutex_);

  if (kSuccess != core_player_->SetSecondSource(fd, offset, length, mime_type)) {
    MV_LOGE("Failed to set source for FF player");
    return false;
  }

  MV_LOGI("LEAVE");
  return true;
};


status_t MRVLFFPlayer::prepare() {
  MV_LOGI("ENTER");

  prepareAsync();

  int retryCount = 0;
  int sleepTime = 10000;
  while ((cur_state_ != State_Stopped) && (retryCount < 100)) {
    usleep(sleepTime);
    retryCount ++;
  }

  MV_LOGI("LEAVE");
  return (cur_state_ == State_Stopped) ? NO_ERROR : UNKNOWN_ERROR;
}

status_t MRVLFFPlayer::prepareAsync() {
  MV_LOGI("ENTER");

  if (!wakeup_event_.successfulInit()) {
    MV_LOGE("failed to initialize wakeup event.");
    return UNKNOWN_ERROR;
  }

  MmuAutoLock lock(action_mutex_);
  cur_state_ = State_Preparing;
  setNextState_l(State_Stopped);
  int res = pthread_create(&work_thread_id_, NULL, staticWorkThreadEntry, this);
  if (res) {
    MV_LOGE("MRVLFFPlayer failed to create its work thread (err = %d)", res);
    return UNKNOWN_ERROR;
  }

  MV_LOGI("LEAVE");
  return NO_ERROR;
}

status_t MRVLFFPlayer::start() {
  MV_LOGI("ENTER");

  if (cur_state_ == State_Playing) {
    MV_LOGI("cur_state_ is alreay State_Playing, don't set again.");
    return NO_ERROR;
  }

  MmuAutoLock lock(action_mutex_);
  setNextState_l(State_Playing);
  wakeup_event_.setEvent();

  mCondition_.wait(action_mutex_);

  MV_LOGI("LEAVE");
  if (State_Playing == cur_state_) {
    return NO_ERROR;
  } else {
    return UNKNOWN_ERROR;
  }
}

status_t MRVLFFPlayer::pause() {
  MV_LOGI("ENTER");

  MmuAutoLock lock(action_mutex_);
  setNextState_l(State_Paused);
  wakeup_event_.setEvent();

  mCondition_.wait(action_mutex_);

  MV_LOGI("LEAVE");
  if ((State_Paused == cur_state_)) {
    return NO_ERROR;
  } else {
    return UNKNOWN_ERROR;
  }
}

status_t MRVLFFPlayer::stop() {
  MV_LOGI("ENTER");
  MmuAutoLock lock(action_mutex_);
  setNextState_l(State_Stopped);
  wakeup_event_.setEvent();
  MV_LOGI("LEAVE");
  return NO_ERROR;
}

status_t MRVLFFPlayer::reset() {
  MV_LOGI("ENTER");

  // Stop CorePlayer datasource firstly so that reset() won't block,
  // especially at preapare stage for HLS.
  core_player_->stopDataSource();

  // Destroy work thread, which also reset CorePlayer.
  if (work_thread_id_) {
    void* thread_ret;

    action_mutex_.lock();
    clearAllStateRequests();
    setNextState_l(State_Shutdown);
    action_mutex_.unlock();
    MV_LOGI("wait MRVLFFPlayer thread finished.");
    pthread_join(work_thread_id_, &thread_ret);
    MV_LOGI("now work thread is already shut down.");
    work_thread_id_ = 0;
  }

  MV_LOGI("LEAVE");
  return NO_ERROR;
}

bool MRVLFFPlayer::isPlaying() {
  bool ret = false;
  if (is_eos_sent_) {
    ret = false;
  } else {
    ret = (State_Playing == cur_state_);
  }
  MV_LOGV("it is %s playing.", ret ? "" : "not");
  return ret;
}

status_t  MRVLFFPlayer::seekTo(int msec) {
  if (1.0 != cur_play_speed_) {
    MV_LOGE("it is during trick play, seek is not supported");
    if (cb_target_) {
      cb_target_->sendMsg(MEDIA_SEEK_COMPLETE);
    }
    return NO_ERROR;
  }
  MV_LOGI("Seek to "TIME_FORMAT"(%d ms).", TIME_ARGS(msec * 1000), msec);
  MmuAutoLock lock(action_mutex_);
  pending_seek_ = true;
  pending_seek_target_ = msec;
  wakeup_event_.setEvent();
  MV_LOGI("LEAVE");
  return NO_ERROR;
}

bool MRVLFFPlayer::setLooping(bool loop) {
  MV_LOGI("loop = %s", (loop!=0) ? "TRUE" : "FALSE");
  MmuAutoLock lock(action_mutex_);
  is_loopback_enabled_ = loop;
  return true;
}

bool MRVLFFPlayer::getLooping() {
  MV_LOGI("loop = %s", is_loopback_enabled_ ? "TRUE" : "FALSE");
  return is_loopback_enabled_;
}

status_t MRVLFFPlayer::getCurrentPosition(int *msec) {
  uint64_t  pos = 0;

  MmuAutoLock lock(action_mutex_);

  *msec = 0;

  if (cur_state_ == State_Preparing) {
    MV_LOGI("cur_state_ is State_Preparing, return directly");
    return NO_ERROR;
  }

  pos = core_player_->GetPosition();
  *msec = (int)(pos / 1000);
  if(*msec < 0) {
    *msec = 0;
  }

  MV_LOGV("Current position: "TIME_FORMAT, TIME_ARGS(pos));
  return NO_ERROR;
}

status_t MRVLFFPlayer::getDuration(int *msec) {
  uint64_t  dur;

  *msec = 0;
  dur = core_player_->GetDuration();
  *msec = (int)(dur / MSToUs);
  if(*msec < 0) {
    *msec = 0;
  }
  return NO_ERROR;
}

status_t MRVLFFPlayer::setVolume(float leftVolume, float rightVolume) {
  core_player_->setVolume(leftVolume, rightVolume);
  return NO_ERROR;
}

status_t MRVLFFPlayer::fastforward(int speed) {
  MmuAutoLock lock(action_mutex_);
  tgt_play_speed_ = (double)speed;
  MV_LOGI("set tgt_play_speed_ %f", tgt_play_speed_);
  return NO_ERROR;
}

status_t MRVLFFPlayer::slowforward(int speed) {
  double slowspeed;
  slowspeed = 1.0;
  slowspeed /= speed;
  MmuAutoLock lock(action_mutex_);
  tgt_play_speed_ = (double)slowspeed;
  MV_LOGI("set tgt_play_speed_ %f", tgt_play_speed_);
  return NO_ERROR;
}

status_t MRVLFFPlayer::fastbackward(int speed) {
  double fastspeed;
  fastspeed = -(double)speed;
  MmuAutoLock lock(action_mutex_);
  tgt_play_speed_ = fastspeed;
  MV_LOGI("set tgt_play_speed_ %f", tgt_play_speed_);
  return NO_ERROR;
}

uint32_t MRVLFFPlayer::GetAudioSubStreamCounts() {
  MV_LOGI("ENTER");
  if(core_player_ != NULL) {
    return core_player_->GetAudioStreamCount();
  } else {
    return 0;
  }
}

uint32_t MRVLFFPlayer::GetVideoSubStreamCounts() {
  MV_LOGI("ENTER");
  if(core_player_ != NULL) {
    return core_player_->GetVideoStreamCount();
  } else {
    return 0;
  }
}

int32_t MRVLFFPlayer::GetCurrentAudioSubStream() {
  MV_LOGI("ENTER");
  MmuAutoLock lock(action_mutex_);
  if (core_player_) {
    return core_player_->GetCurrentAudioStream();
  } else {
    return -1;
  }

}

bool MRVLFFPlayer::getAudioStreamsDescription(streamDescriptorList *desc_list) {
  MV_LOGI("ENTER");
  bool ret = false;
  MmuAutoLock lock(action_mutex_);
  if (!desc_list || !core_player_) {
    ret = false;
  }
  if (core_player_) {
    ret = core_player_->getAudioStreamsDescription(desc_list);
  }
  return ret;
}

status_t MRVLFFPlayer::SetCurrentAudioSubStream(int sub_stream_id) {
  MV_LOGI("ENTER");
  int current_audio = 0;
  int totalaudio_count = 0;

  MmuAutoLock lock(action_mutex_);
  totalaudio_count = core_player_->GetAudioStreamCount();

  if ( totalaudio_count == 0) {
    MV_LOGI("This file just has %d audio stream!!!", totalaudio_count);
    return UNKNOWN_ERROR;
  }else if ( totalaudio_count == 1) {
    return NO_ERROR;
  }else if ( sub_stream_id < 0 && sub_stream_id >= totalaudio_count) {
    MV_LOGI("This audio id is invalid, please set audioID from 0 to %d!!",
        totalaudio_count- 1);
    return UNKNOWN_ERROR;
  } else {
    // set current audio streams ;
    core_player_->SelectAudioStream(sub_stream_id);
    current_audio = core_player_->GetCurrentAudioStream();
    MV_LOGI("setcurrentAudiotrackID( cur %d) done( total %d)",
        current_audio, totalaudio_count);
  }

  MV_LOGI("EXIT");
  return NO_ERROR;
}

uint32_t MRVLFFPlayer::GetSubtitleSubStreamCounts() {
  MV_LOGI("ENTER");
  if(core_player_ != NULL)
    return core_player_->GetSubtitleStreamCount();
  else
    return 0;
}

int32_t MRVLFFPlayer::GetCurrentSubtitleSubStream() {
  MV_LOGI("ENTER");
  MmuAutoLock lock(action_mutex_);
  if (core_player_) {
    return core_player_->GetCurrentSubtitleStream();
  } else {
    return -1;
  }
}

bool MRVLFFPlayer::getSubtitleStreamsDescription(streamDescriptorList *desc_list) {
  MV_LOGI("ENTER");
  bool ret = false;
  MmuAutoLock lock(action_mutex_);
  if (!desc_list || !core_player_) {
    ret = false;
  }
  if (core_player_) {
    ret = core_player_->getSubtitleStreamsDescription(desc_list);
  }
  return ret;
}

bool MRVLFFPlayer::needCheckTimeOut() {
#if 0
  // Only in local media play back we do not check timeout
  if (core_player_ && CorePlayer::SOURCE_FILE == core_player_->getDataSourceType()) {
    return false;
  } else {
    return true;
  }
#else
  return true;
#endif
}


status_t MRVLFFPlayer::SetCurrentSubtitleSubStream(int subid) {
  MV_LOGI("ENTER");
  bool ret = false;
  int current_sub = 0;
  int allsub_count = 0;

  MmuAutoLock lock(action_mutex_);
  allsub_count = core_player_->GetSubtitleStreamCount();
  if ( allsub_count == 0) {
    MV_LOGI("This file no subtitle!!!\n");
    return UNKNOWN_ERROR;
  } else if ( subid <= -1 && subid >= allsub_count) {
    MV_LOGI("This subtitle id is invalid, please set SubID from -1 to %d!!",
        allsub_count- 1);
    return UNKNOWN_ERROR;
  } else {
    // set current subtitle ;
    core_player_->SelectSubtitleStream(subid);
    current_sub = core_player_->GetCurrentSubtitleStream();
    MV_LOGI("setcurrentSubtitletrackID(cur %d) done(total %d)",
        current_sub, allsub_count);
  }
  ret = NO_ERROR;
exit:
  return NO_ERROR;
}

status_t MRVLFFPlayer::DeselectSubtitleSubStream(int subid) {
  MV_LOGI("ENTER");
  status_t ret = NO_ERROR;
  int current_sub = 0;
  int allsub_count = 0;

  MmuAutoLock lock(action_mutex_);
  allsub_count = core_player_->GetSubtitleStreamCount();
  if ( allsub_count == 0) {
    MV_LOGI("This file no subtitle!!!\n");
    return UNKNOWN_ERROR;
  } else if ( subid <= -1 && subid >= allsub_count) {
    MV_LOGI("This subtitle id is invalid, please set SubID from -1 to %d!!",
        allsub_count- 1);
    return UNKNOWN_ERROR;
  } else {
    // set current subtitle ;
    MediaResult err = core_player_->DeselectSubtitleStream(subid);
    if (err != kSuccess) {
      ret = UNKNOWN_ERROR;
    }
  }

exit:
  return ret;
}


// It should be called after lock is held.
void MRVLFFPlayer::setNextState_l(State state) {
  // We simply ignore overflow of state queue because there won't be
  // many state change in a short time.
  state_queue_[end_++] = state;
  if (end_ >= QUEUE_LEN) {
    end_ = 0;
  }
}

MRVLFFPlayer::State MRVLFFPlayer::getNextState() {
  State next = State_Invalid;

  MmuAutoLock lock(action_mutex_);

  if (front_ == end_) {
    return State_Invalid;
  }

  next = state_queue_[front_++];
  if (front_ >= QUEUE_LEN) {
    front_ = 0;
  }
  return next;
}

// It should be called after lock is held.
bool MRVLFFPlayer::clearAllStateRequests() {
  // Remove state transition requests.
  front_ = 0;
  end_ = 0;
  for (int i = 0; i < QUEUE_LEN; i++) {
    state_queue_[i] = State_Invalid;
  }

  // Remove seek request.
  pending_seek_ = false;
  pending_seek_target_ = 0;

  return true;
}

bool MRVLFFPlayer::transferFromPreparing() {
  MmuAutoLock lock(action_mutex_);

  switch (tgt_state_) {
    case State_Stopped:
      break;

    case State_Shutdown:
      return true;

    default:
      return false;
  }

  bool ret = false;
  if (kSuccess == core_player_->Prepare()) {
    MV_LOGI("Core player is prepared, send MEDIA_PREPARED");
    if (cb_target_) {
      cb_target_->sendMsg(MEDIA_PREPARED);
    }

    cur_state_ = State_Stopped;
    ret = true;
  } else {
    setNextState_l(State_Shutdown);
    if (cb_target_) {
      cb_target_->sendMsg(MEDIA_ERROR, 100, MRVL_PLAYER_ERROR_PROBE_FILE_FAILED);
    }
    MV_LOGE("work thread, CorePlayer meet error when prepare");
  }

  return ret;
}

bool MRVLFFPlayer::transferFromStopped() {
  MmuAutoLock lock(action_mutex_);

  switch (tgt_state_) {
    case State_Playing:
      break;

    case State_Paused:
      cur_state_ = State_Paused;
      return true;

    case State_Shutdown:
      return true;

    default:
      return false;
  }

  bool ret = false;
  if (kSuccess == core_player_->Play(1.0)) {
    cur_play_speed_ = 1.0;
    tgt_play_speed_ = 1.0;
    cur_state_ = State_Playing;
    is_eos_sent_ = false;
    ret = true;
  }
  mCondition_.broadcast();

  return ret;
}

bool MRVLFFPlayer::transferFromPaused() {
  MmuAutoLock lock(action_mutex_);

  switch (tgt_state_) {
    case State_Playing:
      break;

    case State_Stopped:
      cur_state_ = State_Stopped;
      return (kSuccess == core_player_->Stop());

    case State_Shutdown:
      return true;

    default:
      return false;
  }

  bool ret = false;
  if (kSuccess == core_player_->Resume(1.0)) {
    cur_play_speed_ = 1.0;
    tgt_play_speed_ = 1.0;
    cur_state_ = State_Playing;
    ret = true;
  }
  mCondition_.broadcast();

  return ret;
}

bool MRVLFFPlayer::transferFromPlaying() {
  MmuAutoLock lock(action_mutex_);
  bool ret = false;

  switch (tgt_state_) {
    case State_Stopped:
      cur_state_ = State_Stopped;
      return (kSuccess == core_player_->Stop());

    case State_Paused:
      ret = (kSuccess == core_player_->Pause());
      cur_state_ = State_Paused;
      mCondition_.broadcast();
      return ret;

    case State_Shutdown:
      return true;

    default:
      return false;
  }
}

bool MRVLFFPlayer::processSeek() {
  MmuAutoLock lock(action_mutex_);

  MV_LOGI("ENTER");

  bool ret = false;
  seek_in_progress_ = true;
  if (kSuccess == core_player_->SetPosition((uint64_t)pending_seek_target_  * MSToUs)) {
    MV_LOGI("seek success");
    is_eos_sent_ = false;
    ret = true;
  } else {
    seek_in_progress_ = false;
    MV_LOGE("seek failed");
  }
  pending_seek_ = false;
  pending_seek_target_ = 0;

  MV_LOGI("EXIT");
  return ret;
}

bool MRVLFFPlayer::processTrickPlay() {
  double play_speed = 1.0;
  {
    // No need to protect the whole pipeline operation.
    MmuAutoLock lock(action_mutex_);
    play_speed = tgt_play_speed_;
  }

  bool ret = false;
  if (kSuccess == core_player_->Play(play_speed)) {
    cur_play_speed_ = play_speed;
    ret = true;
  } else {
    MV_LOGE("calling trick play failed");
  }
  return ret;
}

bool MRVLFFPlayer::waitingForWork(int msecTimeout) {
  struct pollfd wait_fd;

  wait_fd.fd = wakeup_event_.getWakeupHandle();
  wait_fd.events = POLLIN;
  wait_fd.revents = 0;

  int res = poll(&wait_fd, 1, msecTimeout);

  wakeup_event_.clearPendingEvents();

  if (res < 0) {
    MV_LOGE("Wait error in PipeEvent, sleeping to prevent overload!");
    usleep(1000);
  }

  return (res > 0);
}

void* MRVLFFPlayer::MRVLFFPlayerWorkThread() {
  bool abort_work_thread = false;
  const int sleepTime = 10; // 10ms for waitingForWork()

  // 20ms
  const uint64_t PLAY_TIME_DELTA = 20 * 1000 * 1000;

  // play position
  uint64_t last_position = 0;
  uint64_t current_position = 0;

  // 200ms
  const uint64_t Delta = 200 * 1000;
  // 5s
  const int Delta_Underrun = 5 * 1000 * 1000;
  // time past
  struct timeval last;
  struct timeval current;
  uint64_t timpe_past = 0;

  int underrun_times = 0;
  struct timeval underrun_time;
  uint64_t timpe_past_from_underrun = 0;

  gettimeofday(&last, NULL);
  gettimeofday(&underrun_time, NULL);

  tgt_state_ = getNextState();
  while ((tgt_state_ != State_Shutdown) && !abort_work_thread) {
    if ((State_Invalid == tgt_state_) || (cur_state_ == tgt_state_)) {
      if (pending_seek_ && (cur_state_ >= State_Stopped)) {
        if (!processSeek()) {
          MV_LOGE("Failed to process pending seek to %d mSec.", pending_seek_target_);
          abort_work_thread = true;
        }
      } else if ((cur_play_speed_ != tgt_play_speed_) &&
          (State_Playing == cur_state_ || State_Paused == cur_state_)) {
        if (!processTrickPlay()) {
          MV_LOGE("Failed to process trick play from %f to %f.",
              cur_play_speed_, tgt_play_speed_);
          abort_work_thread = true;
        }
      } else {
        // Check whether underrun happens every 200ms.
        if (!seek_in_progress_) {
          gettimeofday(&current, NULL);
          timpe_past += (current.tv_sec - last.tv_sec) * 1000000 + (current.tv_usec - last.tv_usec);
          last.tv_sec  = current.tv_sec;
          last.tv_usec = current.tv_usec;
        } else {
          gettimeofday(&last, NULL);
          timpe_past = 0;
        }
        if (timpe_past >= Delta) {
          timpe_past = 0;
          bool datasource_underrun = core_player_->isUnderrun();
        }
        // Waiting for new task.
        waitingForWork(sleepTime);
      }
    } else {
      MV_LOGD("transfer state cur state = %s, target state = %s",
          State2Str(cur_state_), State2Str(tgt_state_));
      switch (cur_state_) {
        case State_Preparing:
          if (!transferFromPreparing()) {
            MV_LOGE("Unexpected error transitioning from %s to %s",
                State2Str(cur_state_), State2Str(tgt_state_));
            abort_work_thread = true;
          }
          break;

        case State_Stopped:
          if (!transferFromStopped()) {
            MV_LOGE("Unexpected error transitioning from %s to %s",
                State2Str(cur_state_), State2Str(tgt_state_));
            abort_work_thread = true;
          }
          break;

        case State_Paused:
          if (!transferFromPaused()) {
            MV_LOGE("Unexpected error transitioning from %s to %s",
                State2Str(cur_state_), State2Str(tgt_state_));
            abort_work_thread = true;
          }
          break;

        case State_Playing:
          if (!transferFromPlaying()) {
            MV_LOGE("Unexpected error transitioning from %s to %s",
                State2Str(cur_state_), State2Str(tgt_state_));
            abort_work_thread = true;
          }
          break;

        case State_Shutdown:
          break;

        case State_WaitingForPrepare:
        default:
          MV_LOGE("Unexpected cur_state = %d in work thread; aborting!", cur_state_);
          abort_work_thread = true;
      }
    }

    tgt_state_ = getNextState();
  }

  // CorePlayer pipeline is created in this thread, so it is proper to destoy it when exit thread.
  if (core_player_) {
    core_player_->Reset();
  }

  if (abort_work_thread && cb_target_) {
    cb_target_->sendMsg(MEDIA_ERROR, 100, MRVL_PLAYER_ERROR_PLAYER_ERROR);
  }

  MV_LOGD("quit MRVLFFPlayerWorkThread now");
  cur_state_ = State_Shutdown;
  mCondition_.broadcast();
  return NULL;
}

/* static */
void* MRVLFFPlayer::staticWorkThreadEntry(void* thiz) {
  if (thiz == NULL) {
    return NULL;
  }
  MRVLFFPlayer *pMRVLFFPlayer = reinterpret_cast<MRVLFFPlayer*>(thiz);
  // Give our thread a name to assist in debugging.
  prctl(PR_SET_NAME, "MRVLFFPlayerWorkThread", 0, 0, 0);

  return pMRVLFFPlayer->MRVLFFPlayerWorkThread();
}

}
