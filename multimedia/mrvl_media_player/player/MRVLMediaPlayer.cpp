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
#include <pthread.h>
}

#include <utils/Log.h>
#include <media/Metadata.h>
#include <binder/Parcel.h>
#include <AudioSystem.h>
#include <binder/IPCThreadState.h>

#include "gfx_type.h"
#ifdef GRALLOC_EXT
#include "gralloc_ext.h"
#endif
#include "streamDescriptor.h"
#include "MediaPlayerOnlineDebug.h"
#include "MRVLMediaPlayer.h"

#undef  LOG_TAG
#define LOG_TAG "MRVLMediaPlayer"

namespace android {

// For request from browser, it is better to play with non-tunnel mode and video
// pictures are rendered by GPU in order to support HTML5 playback.
// For request from other app(not browser), it is better to play with tunnel mode
// with better performance.
static bool isFromBrowser() {
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
  // TODO: add more browser command line here.
  const char* browsers[] = {
    "com.android.browser",
    "com.android.chrome",
    "com.baidu.browser.apps",
    "com.tencent.mtt",
    "com.UCMobile",
  };

  char buf[256];
  memset(buf, 0x00, 256);
  int pid = IPCThreadState::self()->getCallingPid();
  snprintf(buf, sizeof(buf)-1, "/proc/%d/cmdline", pid);
  FILE *file = fopen(buf, "r");
  if (file == NULL) {
    ALOGD("%s() line %d, can't open %s", __FUNCTION__, __LINE__, buf);
    return false;
  }
  buf[0] = 0;
  fread(buf, 1, sizeof(buf), file);
  fclose(file);
  ALOGD("%s() line %d, buf = %s", __FUNCTION__, __LINE__, buf);

  bool fromBrowser = false;
  for (int i = 0; i < NELEM(browsers); i++) {
    if (!strcmp(buf, browsers[i])) {
      ALOGD("%s() line %d, the request is from browser %s",
          __FUNCTION__, __LINE__, browsers[i]);
      fromBrowser = true;
      break;
    }
  }

  if (!fromBrowser) {
    if (strstr("browser", buf) || strstr("chrome", buf)) {
      ALOGD("%s() line %d, the request is from browser %s",
          __FUNCTION__, __LINE__, buf);
      fromBrowser = true;
    }
  }

  return fromBrowser;
}

MediaPlayerBase* createMRVLMediaPlayer() {
  return new MRVLMediaPlayer();
}

MRVLMediaPlayer::MRVLMediaPlayer()
  : mrvl_player_(NULL),
    audioflinger_handle_(-1),
    is_output_started(false),
    video_plane_id_(0),
    audio_channel_id_(0),
    width_(0),
    height_(0),
    mList_(NULL),
    token_id_(0),
    timeout_thread_(0),
    quit_thread_(false),
    resource_ready_(false),
    left_vol_(0.0),
    right_vol_(0.0),
    set_vol_delay_(false),
    bufferProducer_(NULL),
    surface_id_(-1),
    omx_tunnel_mode_(OMX_TUNNEL) {
  MRVL_PLAYER_LOGD("Begin to create MRVLMediaPlayer.");
#ifdef ANDROID_A3CE
  audioflinger_handle_ = android::AudioSystem::getOutput(AUDIO_STREAM_MUSIC);
#else
  audioflinger_handle_ = android::AudioSystem::getOutput(android::AudioSystem::MUSIC);
#endif

  // Start timeout monitor thread.
  int res = pthread_create(&timeout_thread_, NULL, staticTimeoutThread, this);
  if (res) {
    MRVL_PLAYER_LOGE("MRVLMediaPlayer failed to create its work thread (err = %d)", res);
  }

// TODO: enable it after non-tunnel mode is ready.
#if 0
  if (isFromBrowser()) {
    omx_tunnel_mode_ = OMX_NON_TUNNEL;
  }
#endif

  MRVL_PLAYER_LOGD("MRVLMediaPlayer is created.");
}

status_t MRVLMediaPlayer::initCheck() {
  return OK;
}

MRVLMediaPlayer::~MRVLMediaPlayer() {
  MRVL_PLAYER_LOGD("Begin to destroy MRVLMediaPlayer.");

  // NOTE: check if we need to call stop and clean the ctx
  if (mrvl_player_) {
    delete mrvl_player_;
    mrvl_player_ = NULL;
  } else {
    MRVL_PLAYER_LOGW("mrvl ff player is not start yet");
  }

  if (is_output_started) {
#ifdef ANDROID_A3CE
    android::AudioSystem::stopOutput(audioflinger_handle_, AUDIO_STREAM_MUSIC);
#else
    android::AudioSystem::stopOutput(audioflinger_handle_, android::AudioSystem::MUSIC);
#endif
    is_output_started = false;
  }

  quit_thread_ = true;
  if (timeout_thread_ > 0) {
    void* thread_ret;
    pthread_join(timeout_thread_, &thread_ret);
    MRVL_PLAYER_LOGD("Now time out monitor thread is already shut down.");
    timeout_thread_ = 0;
  }

  list_mutex_.lock();
  TimeoutObjectList *tmp_node = NULL;
  while (mList_) {
    tmp_node = mList_;
    mList_ = mList_->next;

    if (tmp_node->obj) {
      free(tmp_node->obj);
      tmp_node->obj = NULL;
    }
    free(tmp_node);
    tmp_node = NULL;
  }
  list_mutex_.unlock();

  bufferProducer_ = NULL;

  MRVL_PLAYER_LOGD("MRVLMediaPlayer is destroyed.");
}


status_t    MRVLMediaPlayer::setDataSource(
            const char *url,
            const KeyedVector<String8, String8> *headers) {
  bool is_multicast = false;
  status_t ret = android::UNKNOWN_ERROR;
  int token = 0;

  MRVL_PLAYER_LOGD("url = %s, headers = %p", url, headers);

  // Start to monitor API time out.
  token = token_id_;
  android_atomic_inc(&token_id_);
  TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
  if (new_obj) {
    new_obj->token = token;
    new_obj->name = (char *)"setDataSource";
    new_obj->timeout_us = 10 * 1000 * 1000; // Time out in 10s
    new_obj->send_kill = false;
    registerTimeout(new_obj);
  }

  if (mrvl_player_) {
    delete mrvl_player_;
    mrvl_player_ = NULL;
  }
  if (!mrvl_player_) {
    mrvl_player_ = new MRVLFFPlayer(this);
  }

  if (!mrvl_player_->setDataSource(url,headers)) {
    MRVL_PLAYER_LOGD("url ffmpeg:Cannot set data source\n");
    delete mrvl_player_;
    mrvl_player_ = NULL;
    goto EXIT;
  }

  ret = OK;

EXIT:
  // Finish monitor time out.
  unregisterTimeout(token);
  return ret;
}

status_t MRVLMediaPlayer::setDataSource(int fd, int64_t offset, int64_t length) {
  status_t ret = android::UNKNOWN_ERROR;
  int token = 0;

  // Convert file path and name from fd, this is useful in debug.
  const char* const proc_link_url = "/proc/self/fd";
  long path_len = pathconf("/", _PC_PATH_MAX);
  if (path_len > 0) {
    path_len++;
    // 1 byte for trailing NULL + 8 bytes for fd string (more than ever needed)
    int fd_path_max_strsize = strlen(proc_link_url) + 1 + 8;
    char proc_path[fd_path_max_strsize];
    snprintf(proc_path, fd_path_max_strsize, "%s/%d", proc_link_url, fd);
    char* file_path = new char[path_len];
    if (file_path) {
      memset(file_path, 0x00, path_len);
      int res = readlink(proc_path, file_path, path_len);
      if ((res > 0) && (res <= path_len)) {
        file_path[res] = '\0';
        MRVL_PLAYER_LOGD("Converted file path from fd: %s", file_path);
      }
      delete[] file_path;
    }
  }

  // Start to monitor API time out.
  token = token_id_;
  android_atomic_inc(&token_id_);
  TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
  if (new_obj) {
    new_obj->token = token;
    new_obj->name = (char *)"setDataSource";
    new_obj->timeout_us = 10 * 1000 * 1000; // Time out in 10s
    new_obj->send_kill = false;
    registerTimeout(new_obj);
  }

  if (mrvl_player_) {
    delete mrvl_player_;
    mrvl_player_ = NULL;
  }
  if (!mrvl_player_) {
    mrvl_player_ = new MRVLFFPlayer(this);
  }
  // NOTE: in the mrvl_player_ it will go to dup(fd), so no need to dup(fd) here
  if(!mrvl_player_->setDataSource(fd, offset, length)) {
    MRVL_PLAYER_LOGD("fd ffmpeg:Cannot set data source\n");
    delete mrvl_player_;
    goto EXIT;
  }

  MRVL_PLAYER_LOGV("fd: %i, offset: %ld, len: %ld\n", fd, (long)offset, (long)length);

  ret = OK;

EXIT:
  // Finish monitor time out.
  unregisterTimeout(token);
  return ret;
}

void MRVLMediaPlayer::flushSurfaceTextureInternal() {
  MRVL_PLAYER_LOGD("flushSurfaceTextureInternal(): this=%p, bufferProducer_.get()=%p",
      this,bufferProducer_ != NULL ? bufferProducer_.get() : 0);

  if (bufferProducer_ == NULL) {
    MRVL_PLAYER_LOGE("bufferProducer_ is NULL");
    return;
  }

  sp<android::Surface> client(new android::Surface(bufferProducer_));
  ANativeWindowBuffer_t* buffer = NULL;
  ANativeWindow* window = client.get();

#ifdef GRALLOC_EXT
  native_window_set_usage(window, GRALLOC_USAGE_EXTERNAL_SURFACE);
#endif

  MRVL_PLAYER_LOGD("video_plane_id_ = [%d]", video_plane_id_);

#ifdef GRALLOC_EXT
  if (video_plane_id_ ==  MV_GFX_2D_CHANNEL_PRIMARY_VIDEO ) {
    surface_id_ = GRALLOC_USAGE_SURFACE_ID_0;
  } else if (video_plane_id_ == MV_GFX_2D_CHANNEL_SECONDARY_VIDEO) {
    surface_id_ = GRALLOC_USAGE_SURFACE_ID_1;
  } else {
    MRVL_PLAYER_LOGE("Error! set invalid surface ID");
    return;
  }
#endif

  MRVL_PLAYER_LOGD("surface_id_=[%x]", video_plane_id_);
  native_window_set_buffers_format(window, surface_id_);
  native_window_set_scaling_mode(window, NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
  window->dequeueBuffer_DEPRECATED(window, &buffer);
  window->queueBuffer_DEPRECATED(window, buffer);

  return;
}

#if PLATFORM_SDK_VERSION < 18
status_t MRVLMediaPlayer::setVideoSurfaceTexture(const sp<ISurfaceTexture>& bufferProducer) {
#else
status_t MRVLMediaPlayer::setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer) {
#endif
  MRVL_PLAYER_LOGD("setVideoSurfaceTexture(): bufferProducer.get()=%p",
      bufferProducer != NULL ? bufferProducer.get() : 0);
  MmuAutoLock bufferProducer_lock(bufferProducer_lock_);
  if (bufferProducer != NULL && bufferProducer == bufferProducer_) {
    // Prevent the same surface from being assigned twice.
    return android::OK;
  }
  bufferProducer_ = bufferProducer;
  if (OMX_TUNNEL == omx_tunnel_mode_) {
    // Pin video hole as far as we get bufferProducer, otherwise we will see wallpaper.
    flushSurfaceTextureInternal();
  } else if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    mrvl_player_->setNativeWindow(new Surface(bufferProducer));
  }
  return android::OK;
}

status_t MRVLMediaPlayer::prepare() {
  mrvl_player_->setOmxTunnelMode(omx_tunnel_mode_);
  return mrvl_player_->prepare();
}

status_t MRVLMediaPlayer::prepareAsync() {
  mrvl_player_->setOmxTunnelMode(omx_tunnel_mode_);
  return mrvl_player_->prepareAsync();
}

status_t MRVLMediaPlayer::start() {
  MRVL_PLAYER_LOGD("ENTER");
  int token = 0;
  status_t ret = OK;

  // Start to monitor API time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    token = token_id_;
    android_atomic_inc(&token_id_);
    TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
    if (new_obj) {
      new_obj->token = token;
      new_obj->name = (char *)"start";
      new_obj->timeout_us = 10 * 1000 * 1000; // Time out in 10s
      new_obj->send_kill = false;
      registerTimeout(new_obj);
    }
  }

  if (!is_output_started) {
#ifdef ANDROID_A3CE
    android::AudioSystem::startOutput(audioflinger_handle_, AUDIO_STREAM_MUSIC);
#else
    android::AudioSystem::startOutput(audioflinger_handle_, android::AudioSystem::MUSIC);
#endif
    is_output_started = true;
  }

  ret = mrvl_player_->start();

  // Finish monitor time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    unregisterTimeout(token);
  }

  MRVL_PLAYER_LOGD("LEAVE");
  return ret;
}

status_t MRVLMediaPlayer::stop() {
  int token = 0;
  status_t ret = OK;

  // Start to monitor API time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    token = token_id_;
    android_atomic_inc(&token_id_);
    TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
    if (new_obj) {
      new_obj->token = token;
      new_obj->name = (char *)"stop";
      new_obj->timeout_us = 10 * 1000 * 1000; // Time out in 10s
      new_obj->send_kill = false;
      registerTimeout(new_obj);
    }
  }
  if (is_output_started) {
#ifdef ANDROID_A3CE
    android::AudioSystem::stopOutput(audioflinger_handle_, AUDIO_STREAM_MUSIC);
#else
    android::AudioSystem::stopOutput(audioflinger_handle_, android::AudioSystem::MUSIC);
#endif
    is_output_started = false;
  }
  ret = mrvl_player_->stop();

  // Finish monitor time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    unregisterTimeout(token);
  }
  return ret;
}

status_t MRVLMediaPlayer::pause() {
  int token = 0;
  status_t ret = OK;

  // Start to monitor API time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    token = token_id_;
    android_atomic_inc(&token_id_);
    TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
    if (new_obj) {
      new_obj->token = token;
      new_obj->name = (char *)"pause";
      new_obj->timeout_us = 10 * 1000 * 1000; // Time out in 10s
      new_obj->send_kill = false;
      registerTimeout(new_obj);
    }
  }
  if (is_output_started) {
#ifdef ANDROID_A3CE
    android::AudioSystem::stopOutput(audioflinger_handle_, AUDIO_STREAM_MUSIC);
#else
    android::AudioSystem::stopOutput(audioflinger_handle_, android::AudioSystem::MUSIC);
#endif
    is_output_started = false;
  }
  setVolume(0.0, 0.0);

  ret = mrvl_player_->pause();

  // Finish monitor time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    unregisterTimeout(token);
  }
  return ret;
}

bool MRVLMediaPlayer::isPlaying(){
  return mrvl_player_->isPlaying();
}

status_t MRVLMediaPlayer::seekTo(int msec) {
  int token = 0;
  status_t ret = OK;

  // Start to monitor API time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    token = token_id_;
    android_atomic_inc(&token_id_);
    TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
    if (new_obj) {
      new_obj->token = token;
      new_obj->name = (char *)"seekTo";
      new_obj->timeout_us = 10 * 1000 * 1000; // Time out in 10s
      new_obj->send_kill = false;
      registerTimeout(new_obj);
    }
  }
  ret = mrvl_player_->seekTo(msec);

  // Finish monitor time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    unregisterTimeout(token);
  }
  return ret;
}

status_t MRVLMediaPlayer::reset() {
  MRVL_PLAYER_LOGD("ENTER");
  int token = 0;
  status_t ret = OK;

  // Start to monitor API time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    token = token_id_;
    android_atomic_inc(&token_id_);
    TimeoutObject *new_obj = (TimeoutObject *)malloc(sizeof(TimeoutObject));
    if (new_obj) {
      new_obj->token = token;
      new_obj->name = (char *)"reset";
      new_obj->timeout_us = 5 * 1000 * 1000; // Time out in 5s
      new_obj->send_kill = true;
      registerTimeout(new_obj);
    }
  }
  if (is_output_started) {
#ifdef ANDROID_A3CE
    android::AudioSystem::stopOutput(audioflinger_handle_, AUDIO_STREAM_MUSIC);
#else
    android::AudioSystem::stopOutput(audioflinger_handle_, android::AudioSystem::MUSIC);
#endif
    is_output_started = false;
  }

  if (mrvl_player_) {
    ret = mrvl_player_->reset();
  }

  // Finish monitor time out.
  if (mrvl_player_ && mrvl_player_->needCheckTimeOut()) {
    unregisterTimeout(token);
  }
  MRVL_PLAYER_LOGD("LEAVE");
  return ret;
}

status_t MRVLMediaPlayer::getVideoWidth(int *w){
  *w = width_;
  return OK;
}

status_t MRVLMediaPlayer::getVideoHeight(int *h) {
  *h = height_;
  return OK;
}

status_t MRVLMediaPlayer::getCurrentPosition(int *msec){
  return mrvl_player_->getCurrentPosition(msec);
}

status_t MRVLMediaPlayer::getDuration(int *msec) {
  return mrvl_player_->getDuration(msec);
}

status_t MRVLMediaPlayer::setLooping(int loop) {
  return mrvl_player_->setLooping(loop != 0) ? OK : android::UNKNOWN_ERROR;
}

bool MRVLMediaPlayer::getLooping() {
  return mrvl_player_->getLooping();
}

status_t MRVLMediaPlayer::setParameter(int key, const Parcel &request) {
  MRVL_PLAYER_LOGV("no setParameter\n");
  return OK;
}


status_t MRVLMediaPlayer::getParameter(int key, Parcel *reply) {
  MRVL_PLAYER_LOGV("no setParameter\n");
  return OK;
}

status_t MRVLMediaPlayer::getMetadata(const media::Metadata::Filter& ids, Parcel *records) {
  // TODO: Opencore use this to set whether can pause/seek, how gstreamer judge this information?
  return OK;
}

status_t MRVLMediaPlayer::invoke(const Parcel& request, Parcel *reply) {
  int para = 0, speed = 0, subID = 0, show = 0, color = 0, totalsubcounts = 0;
  int audio_stream = 0, sub_stream = 0;
  int index = 0;
  status_t ret = NO_ERROR;
  para = request.readInt32();
  switch (para) {
    case PARA_FAST_FORWARD:
      speed = request.readInt32();
      return mrvl_player_->fastforward(speed) ? OK : android::UNKNOWN_ERROR;
    case PARA_FAST_BACKWARD:
      speed = request.readInt32();
      return mrvl_player_->fastbackward(speed) ? OK : android::UNKNOWN_ERROR;
    case PARA_SLOW_FORWARD:
      speed = request.readInt32();
      return mrvl_player_->slowforward(speed) ? OK : android::UNKNOWN_ERROR;

    // For audio sub stream switch
    case INVOKE_ID_ENUM_AUDIO_SUBSTREAMS:
      totalsubcounts = mrvl_player_->GetAudioSubStreamCounts();
      if (totalsubcounts <= 0) {
        reply->writeInt32(0);
      } else {
        // num of streams
        reply->writeInt32(totalsubcounts);
        streamDescriptorList des_list;
        memset(&des_list, 0x00, sizeof(streamDescriptorList));
        if (mrvl_player_->getAudioStreamsDescription(&des_list)) {
          for (index = 0; index < totalsubcounts; index++) {
            streamDescriptor *sd = des_list.descriptors.at(index);
            // codec
            reply->writeInt32(0);
            if (sd->mime_type) {
              String8 mime_type(sd->mime_type);
              String16 mime_type16(mime_type);
              reply->writeString16(mime_type16);
            } else {
              char *mime = (char *)"Unknown";
              String8 mime8(mime);
              String16 mime16(mime8);
              reply->writeString16(mime16);
            }
            // language
            if (sd->language) {
              String8 language(sd->language);
              String16 lan16(language);
              reply->writeString16(lan16);
            } else {
              char *lan = (char *)"Unknown";
              String8 language(lan);
              String16 lan16(language);
              reply->writeString16(lan16);
            }
            // description
            if (sd->description) {
              String8 description(sd->description);
              String16 des16(description);
              reply->writeString16(des16);
            } else {
              char *des = (char *)"Unknown";
              String8 description(des);
              String16 des16(description);
              reply->writeString16(des16);
            }
          }
        }
      }
      return OK;

    case INVOKE_ID_GET_CURRENT_AUDIO_SUBSTREAM:
      audio_stream = mrvl_player_->GetCurrentAudioSubStream();
      reply->writeInt32(audio_stream);
      return OK;

    case INVOKE_ID_SET_CURRENT_AUDIO_SUBSTREAM:
      audio_stream = request.readInt32();
      MRVL_PLAYER_LOGD("INVOKE_ID_SET_CURRENT_AUDIO_SUBSTREAM");
      ret = mrvl_player_->SetCurrentAudioSubStream(audio_stream);
      return ret;

    // For subtitle sub stream switch
    case INVOKE_ID_ENUM_SUBTITLE_SUBSTREAMS:
      totalsubcounts = mrvl_player_->GetSubtitleSubStreamCounts();
      if (totalsubcounts <= 0) {
        reply->writeInt32(0);
      } else {
        // num of streams
        reply->writeInt32(totalsubcounts);
        streamDescriptorList des_list;
        memset(&des_list, 0x00, sizeof(streamDescriptorList));
        if (mrvl_player_->getSubtitleStreamsDescription(&des_list)) {
          for (index = 0; index < totalsubcounts; index++) {
            streamDescriptor *sd = des_list.descriptors.at(index);
            // codec
            reply->writeInt32(0);
            if (sd->mime_type) {
              String8 mime(sd->mime_type);
              String16 mime16(mime);
              reply->writeString16(mime16);
            } else {
              char *mime = (char *)"Unknown";
              String8 mime8(mime);
              String16 mime16(mime8);
              reply->writeString16(mime16);
            }
            // language
            if (sd->language) {
              String8 language(sd->language);
              String16 lan16(language);
              reply->writeString16(lan16);
            } else {
              char *lan = (char *)"Unknown";
              String8 language(lan);
              String16 lan16(language);
              reply->writeString16(lan16);
            }
            // description
            if (sd->description) {
              String8 description(sd->description);
              String16 des16(description);
              reply->writeString16(des16);
            } else {
              char *des = (char *)"Unknown";
              String8 description(des);
              String16 des16(description);
              reply->writeString16(des16);
            }
          }
        }
      }
      return OK;

    case INVOKE_ID_GET_CURRENT_SUBTITLE_SUBSTREAM:
      sub_stream = mrvl_player_->GetCurrentSubtitleSubStream();
      reply->writeInt32(sub_stream);
      return OK;

    case INVOKE_ID_SET_CURRENT_SUBTITLE_SUBSTREAM:
      subID = request.readInt32();
      MRVL_PLAYER_LOGD("Set Subtitile to %d", subID);
      if(!mrvl_player_->SetCurrentSubtitleSubStream(subID)) {
        show = 1;
        return OK;
      }else{
        return android::UNKNOWN_ERROR;
      }

    case INVOKE_ID_SUBTITLE_SHOW:
      show = request.readInt32();
      MRVL_PLAYER_LOGD("invoke show subtitle %d", show);
      return OK;

    case INVOKE_ID_SET_SUBTITLE_COLOR:
      color = request.readInt32();
      MRVL_PLAYER_LOGD("invoke set color %d", color);
      return OK;
    //below cases are for CTS
    case INVOKE_ID_GET_TRACK_INFO:
      MRVL_PLAYER_LOGD("invoke get track info");
      return getTrackInfo(reply);

    case INVOKE_ID_UNSELECT_TRACK:
    case INVOKE_ID_SELECT_TRACK: {
      //according to stagefright,only switch audio and subtitle track
      int trackIndex = request.readInt32();
      MRVL_PLAYER_LOGD("invoke select track %d",trackIndex);
      int videoCounts = mrvl_player_->GetVideoSubStreamCounts();
      int audioCounts = mrvl_player_->GetAudioSubStreamCounts();
      int subtitleCounts = mrvl_player_->GetSubtitleSubStreamCounts();
      if (trackIndex < 0 || trackIndex >= videoCounts + audioCounts + subtitleCounts) {
        return INVALID_OPERATION;
      } else if (trackIndex < videoCounts) {//video
        return INVALID_OPERATION;
      } else if (trackIndex >= videoCounts && trackIndex < videoCounts + audioCounts) {//audio
        audio_stream = trackIndex - mrvl_player_->GetVideoSubStreamCounts();
        MRVL_PLAYER_LOGD("invoke select audio track %d",audio_stream);
        ret = mrvl_player_->SetCurrentAudioSubStream(audio_stream);
      } else if (trackIndex >= videoCounts + audioCounts) { //subtitle
        subID = trackIndex - videoCounts -audioCounts ;
        if (para == INVOKE_ID_UNSELECT_TRACK) {
          ret = mrvl_player_->DeselectSubtitleSubStream(subID);
        } else {
          MRVL_PLAYER_LOGD("invoke select subtitle track %d",subID);
          ret = mrvl_player_->SetCurrentSubtitleSubStream(subID);
        }
      }
      return ret;
    }

    case INVOKE_ID_ADD_EXTERNAL_SOURCE : {
      String8 url(request.readString16());
      String8 mimeType(request.readString16());
      ret = mrvl_player_->setSecondDataSource(url.string(), mimeType.string());
      return ret;
    }

    case INVOKE_ID_ADD_EXTERNAL_SOURCE_FD: {
      MRVL_PLAYER_LOGD("INVOKE_ID_ADD_EXTERNAL_SOURCE_FD");
      int fd         = request.readFileDescriptor();
      off64_t offset = request.readInt64();
      off64_t length  = request.readInt64();
      String8 mimeType(request.readString16());
      ret = mrvl_player_->setSecondDataSource(fd, offset, length, mimeType.string());
      return ret;
    }

    case INVOKE_ID_SET_VIDEO_SCALING_MODE:
      MRVL_PLAYER_LOGD("invoke do nothing now,only for pass CTS");
      return OK;

    default:
      return INVALID_OPERATION;
    }

  return INVALID_OPERATION;
}

status_t MRVLMediaPlayer::getTrackInfo(Parcel *reply) const {
  if (!mrvl_player_) {
    MRVL_PLAYER_LOGE("Player is not prepared!");
    return INVALID_OPERATION;
  }
  int videoCounts = mrvl_player_->GetVideoSubStreamCounts();
  int audioCounts = mrvl_player_->GetAudioSubStreamCounts();
  int subtitleCounts = mrvl_player_->GetSubtitleSubStreamCounts();
  size_t totalsubcounts = 0;
  totalsubcounts += videoCounts;
  totalsubcounts += audioCounts;
  totalsubcounts += subtitleCounts;
  MRVL_PLAYER_LOGD("get track info ,totalsubcounts :%d",totalsubcounts);
  // TODO: whether to add ExternalTracks
  /*if (mTextDriver != NULL) {
      trackCount += mTextDriver->countExternalTracks();
  }*/
  reply->writeInt32(totalsubcounts);
  int index=0;
  //Pay attention: shoud keep the reply order:video->audio->subtitle
  for (index = 0; index < videoCounts; index++) {
    reply->writeInt32(2); // 2 fields
    reply->writeInt32(MEDIA_TRACK_TYPE_VIDEO);
    const char *lang;
    lang = "und";// TODO:  Do we need to get video language?
    reply->writeString16(String16(lang));
  }
  streamDescriptorList des_list;
  memset(&des_list, 0x00, sizeof(streamDescriptorList));
  if (mrvl_player_->getAudioStreamsDescription(&des_list)) {
    for (index = 0; index < audioCounts; index++) {
      reply->writeInt32(2); // 2 fields
      reply->writeInt32(MEDIA_TRACK_TYPE_AUDIO);
      streamDescriptor *sd = des_list.descriptors.at(index);
      // language
      if (sd->language) {
        String8 language(sd->language);
        String16 lan16(language);
        reply->writeString16(lan16);
      } else {
        char *lan = (char *)"und";
        String8 language(lan);
        String16 lan16(language);
        reply->writeString16(lan16);
      }
    }
  }
  memset(&des_list, 0x00, sizeof(streamDescriptorList));
  if (mrvl_player_->getSubtitleStreamsDescription(&des_list)) {
    for (index = 0; index < subtitleCounts; index++) {
      reply->writeInt32(2); // 2 fields
      reply->writeInt32(MEDIA_TRACK_TYPE_TIMEDTEXT);
      streamDescriptor *sd = des_list.descriptors.at(index);
      // language
      if (sd->language) {
        String8 language(sd->language);
        String16 lan16(language);
        reply->writeString16(lan16);
      } else {
        char *lan = (char *)"und";
        String8 language(lan);
        String16 lan16(language);
        reply->writeString16(lan16);
      }
    }
  }
  return OK;
}


status_t MRVLMediaPlayer::setVolume(float leftVolume, float rightVolume) {
  MRVL_PLAYER_LOGD("leftVolume=%.2f, rightVolume=%.2f",
      leftVolume, rightVolume);
  mrvl_player_->setVolume(leftVolume, rightVolume);

  return android::OK;
}

void MRVLMediaPlayer::sendMsg(int32_t msg, int ext1, int ext2, void *parcel) {
  MRVL_PLAYER_LOGD("Get msg = %s", Msg2Str(msg));
  if ((msg == MEDIA_INFO) && (ext1 == MSG_RESOURCE_READY)) {
      audio_channel_id_ = ext2 >> 16;
      video_plane_id_ = ext2 & 0xFFFF;
      // We don't pass PE audio/video channel to player.

      if (set_vol_delay_) {
        setVolume(left_vol_, right_vol_);
      }

      if (OMX_TUNNEL == omx_tunnel_mode_) {
        flushSurfaceTextureInternal();
      }
      return;
  }
  if (msg == MEDIA_SET_VIDEO_SIZE ) {
      width_ = ext1;
      height_ = ext2;
  }
  MediaPlayerBase::sendEvent(msg, ext1, ext2, (Parcel *)parcel);
}

bool MRVLMediaPlayer::registerTimeout(TimeoutObject *obj) {
  if (!obj) {
    return false;
  }
  MmuAutoLock lock(list_mutex_);

  TimeoutObjectList *new_node = (TimeoutObjectList *)malloc(sizeof(TimeoutObjectList));
  if (!new_node) {
    MRVL_PLAYER_LOGE("Failed to create a node, maybe out of memory");
    return false;
  }
  new_node->obj = obj;
  new_node->next = NULL;

  // append to mList_
  if (!mList_) {
    mList_ = new_node;
  } else {
    TimeoutObjectList *tmp_node = mList_;
    while (tmp_node && tmp_node->next) {
      tmp_node = tmp_node->next;
    }
    tmp_node->next = new_node;
  }

  return true;
}

bool MRVLMediaPlayer::unregisterTimeout(uint32_t token_id) {
  MmuAutoLock lock(list_mutex_);
  TimeoutObjectList *tmp_node = NULL;
  TimeoutObjectList *ptr = NULL;

  if (!mList_) {
    return true;
  }

  MRVL_PLAYER_LOGV("token_id = %d", token_id);
  tmp_node = mList_;
  while ((tmp_node->obj->token != (int)token_id) && tmp_node->next) {
    ptr = tmp_node;
    tmp_node = tmp_node->next;
  }

  if (tmp_node->obj->token == (int)token_id) {
    if (tmp_node == mList_) {
      // If it is mList_.
      mList_ = tmp_node->next;
    } else {
      ptr->next = tmp_node->next;
    }
    // Delete it from list.
    free(tmp_node->obj);
    tmp_node->obj = NULL;
    free(tmp_node);
    tmp_node = NULL;
  }

  return true;
}

void* MRVLMediaPlayer::TimeoutThreadEntry() {
  const int SleepTime = 10 * 1000; // 10ms

  TimeoutObjectList *tmp_node = NULL;
  TimeoutObjectList *ptr = NULL;

  struct timeval last;
  struct timeval current;
  int time_passed_us = 0;

  while (!quit_thread_) {
    gettimeofday(&last, NULL);
    usleep(SleepTime);
    // Loop time out object list and decrease each by 5ms.
    // If one object time out, notify application then delete it.
    list_mutex_.lock();
    if (!mList_) {
      list_mutex_.unlock();
      continue;
    }

    gettimeofday(&current, NULL);
    time_passed_us = (current.tv_sec - last.tv_sec) * 1000000 + (current.tv_usec - last.tv_usec);

    tmp_node = mList_;
    ptr = mList_;
    while(tmp_node) {
      tmp_node->obj->timeout_us -= time_passed_us;
      if (tmp_node->obj->timeout_us > 0) {
        ptr = tmp_node;
        tmp_node = tmp_node->next;
      } else {
        // This object time out.
        if (tmp_node == mList_) {
          // move header
          mList_ = mList_->next;
          // Notify application and delete the object.
          MRVL_PLAYER_LOGE("The first one(id: %d, name: %s) in list time out",
              tmp_node->obj->token, tmp_node->obj->name);
          sendMsg(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, MRVL_PLAYER_ERROR_API_TIMEOUT);
          // If time out and maybe deadlock, kill myself to break deadlock.
          if (tmp_node->obj->send_kill) {
            //clear video channel
            sendMsg(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, MRVL_PLAYER_ERROR_TIMEOUT_ABORT);
            MRVL_PLAYER_LOGE("Maybe deadlock happened, I have to kill myself to break deadlock.");
            kill(getpid(), SIGKILL);
          }
          free(tmp_node->obj);
          tmp_node->obj = NULL;
          free(tmp_node);
          tmp_node = NULL;
          // set pointer
          tmp_node = mList_;
          ptr = mList_;
        } else {
          // set pointer
          ptr->next = tmp_node->next;
          // Notify application and delete the object.
          MRVL_PLAYER_LOGE("The first one(id: %d, name: %s) in list time out",
              tmp_node->obj->token, tmp_node->obj->name);
          sendMsg(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, MRVL_PLAYER_ERROR_API_TIMEOUT);
          // If time out and maybe deadlock, kill myself to break deadlock.
          if (tmp_node->obj->send_kill) {
            sendMsg(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, MRVL_PLAYER_ERROR_TIMEOUT_ABORT);
            MRVL_PLAYER_LOGE("Maybe deadlock happened, I have to kill myself to break deadlock.");
            kill(getpid(), SIGKILL);
          }
          free(tmp_node->obj);
          tmp_node->obj = NULL;
          free(tmp_node);
          tmp_node = NULL;
          // set pointer
          tmp_node = ptr->next;
        }
      }
    }

    list_mutex_.unlock();
  }

  MRVL_PLAYER_LOGD("Quit time out monitor thread now");
  return NULL;
}

void* MRVLMediaPlayer::staticTimeoutThread(void* thiz) {
  if (thiz == NULL) {
    return NULL;
  }
  MRVLMediaPlayer *pMRVLMediaPlayer = reinterpret_cast<MRVLMediaPlayer*>(thiz);
  // Give our thread a name to assist in debugging.
  prctl(PR_SET_NAME, "MRVLMediaPlayer-timeout-thread", 0, 0, 0);

  return pMRVLMediaPlayer->TimeoutThreadEntry();
}

}
