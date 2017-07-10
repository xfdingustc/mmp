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
#ifndef ANDROID_UNIVERSAL_PLAYER_H
#define ANDROID_UNIVERSAL_PLAYER_H

extern "C" {
#include "stdio.h"
#include "MmpMediaDefs.h"
#include "avformat.h"
}

#include "datasource/RawDataSource.h"
#include "demuxer/RawDataReader.h"

#include "streamDescriptor.h"
#include "MediaPlayerOnlineDebug.h"
#include "omx/MHALOmxComponent.h"
#include "omx/MHALOmxUtils.h"
#include "subtitle/MHALSubtitleEngine.h"
#include "render/AndroidAudioOutput.h"
#include "render/AndroidVideoOutput.h"
#include "render/MmpSystemClock.h"


#ifdef ENABLE_RESOURCE_MANAGER
#include <ResourceClient.h>
#include <IResourceManager.h>
#include "condSignal.h"

using namespace resourcemanager;
#endif

#include <vector>

#include "demuxer/TimedTextDemux.h"

using namespace std;
using namespace android;

namespace mmp {

class MHALAudioDecoder;
class MHALAudioRenderer;
class MHALClock;
class MHALVideoDecoder;
class MHALSubtitleDecoder;
class MHALSubitlteEngine;

typedef enum
{
  MSG_UNKNOWN = 0,
  MSG_EOS,            //playback or ff to the end
  MSG_ERROR,          //critical error during playback
  MSG_STEP_DONE,      //fast backword to the beginning
  MSG_WARNING,        //warning message, such as video/audio is not supported
  MSG_LOOPBACK,       //loopback play
  MSG_RES_CHANGE,     //resolution has changed
  MSG_SEEK_COMPLETE,
  MSG_RESOURCE_READY,
  MSG_VIDEO_NOTSUPPORT,
  MSG_AUDIO_NOTSUPPORT,
  MSG_RENDERING_START,
  MSG_TIMED_TEXT,
  MSG_UNSUPPORT,
  MSG_PLAYING_BANDWIDTH = 803, //This should be unique, because it will be sent to MediaPlayer.java
  MSG_HLS_REALTIME_SPEED = 804,
  MSG_HLS_DOWNLOAD_PROGRESS_IN_PERCENT = 805,
  MSG_HLS_DOWNLOAD_PROGRESS_IN_BYTES = 806,
  MSG_HLS_DOWNLOAD_EOS = 807,
  MSG_HLS_IP_ADDRESS = 808,
  MSG_BUFFERING_UPDATE,
} MSG_TYPE;

enum {
  // Failed to request resource from ResourceManager
  MRVL_PLAYER_ERROR_REQUEST_RESOURCE_FAIL = 0x1001,

  // Error happens in FFmpeg/RMVB demuxer
  MRVL_PLAYER_ERROR_DEMUX_ERROR = 0x1002,

  // Other application steals resource(audio/video decoder, audio channel, video plane) from us
  MRVL_PLAYER_ERROR_RESOURCE_STOLEN = 0x1003,

  // File probe failed, it can't be played.
  MRVL_PLAYER_ERROR_PROBE_FILE_FAILED = 0x1004,

  // When API time out, it sends this error code to application.
  MRVL_PLAYER_ERROR_API_TIMEOUT = 0x2001,

  // When reset() time out, it sends this error code to application and
  // kill itself to break possible deadlock.
  MRVL_PLAYER_ERROR_TIMEOUT_ABORT = 0x2002,

  // Playlist is not download, or it is not a HLS playlist.
  MRVL_PLAYER_ERROR_FAILED_GET_HLS_PLAYLIST = 0x3001,

  // Failed to fetch .ts segment for HLS playback.
  MRVL_PLAYER_ERROR_FAILED_GET_HLS_TS = 0x3002,

  // Other errors in player.
  MRVL_PLAYER_ERROR_PLAYER_ERROR = 0x9000,

  MRVL_PLAYER_ERROR_UNKNOWN,
};

class CorePlayerCbTarget {
  public:
    virtual ~CorePlayerCbTarget() {};
    virtual status_t sendMsg(MSG_TYPE type, void *data = NULL) = 0;
};

class CorePlayer : public MRVLDataSourceCbTarget,
                       public MRVLDataReaderCbTarget,
                       public MRVLSubtitleCbTarget,
#ifdef ENABLE_RESOURCE_MANAGER
                       public RMEventHandler,
#endif
                       public MHALOmxCallbackTarget {
  public:
    explicit CorePlayer(CorePlayerCbTarget *target);
    virtual ~CorePlayer();

    // Callback functions
    virtual MediaResult OnDataSourceEvent(DataSourceEvent what, int extra);
    virtual bool OnSeekComplete(TimeStamp seek_time);
#ifdef ENABLE_RESOURCE_MANAGER
    // Callback for resource allocation. This funcion will be registered to ResourceManager client.
    virtual void handleRMEvent(RMEvent event, ResHandle handle, sp<ResourceDesc>& desc);
#endif
    virtual OMX_BOOL OnOmxEvent(MHAL_EVENT_TYPE EventCode, OMX_U32 nData1 = 0,
                OMX_U32 nData2 = 0, OMX_PTR pEventData = NULL);
    virtual MmpError onSubtitleCome(void *parcel);

    MediaResult SetSource(const char* url, const KeyedVector<String8, String8> *headers = NULL);
    MediaResult SetSource(int32_t fd, uint64_t offset, uint64_t length);
    MediaResult SetSecondSource(const char* url, const char* mime_type);
    MediaResult SetSecondSource(int32_t fd, uint64_t offset, uint64_t length,const char* mime_type);
    MediaResult Prepare();
    MediaResult Play(double rate);
    MediaResult Resume(double rate);
    MediaResult Pause();
    MediaResult Stop();
    MediaResult Reset();
    MediaResult SetPosition(TimeStamp posInUs);
    MediaResult setVolume(float leftVolume, float rightVolume);

    TimeStamp GetPosition();
    TimeStamp GetDuration();

    MediaResult SelectSubtitleStream(int32_t sub_idx);
    MediaResult DeselectSubtitleStream(int32_t sub_idx) ;
    MediaResult SelectAudioStream(int32_t audio_idx);

    int GetVideoStreamCount() { return file_info.supported_video_counts; }
    int GetAudioStreamCount() { return file_info.supported_audio_counts; }
    int GetInternalSubtitleStreamCount() { return file_info.supported_subtitle_counts; }
    int GetExternalSubtitleStreamCount() { return ext_sub_count_; }
    int GetSubtitleStreamCount() {
        return GetInternalSubtitleStreamCount() + GetExternalSubtitleStreamCount();
    }
    int GetCurrentAudioStream() { return audio_id_; }
    int GetCurrentSubtitleStream() { return subtitle_id_; }
    bool getAudioStreamsDescription(streamDescriptorList *desc_list);
    bool getSubtitleStreamsDescription(streamDescriptorList *desc_list);

    inline bool hasAudio() { return file_info.supported_audio_counts > 0; }
    inline bool hasVideo() { return file_info.supported_video_counts > 0; }
    inline bool hasSubtitle() { return GetSubtitleStreamCount() > 0; }

    MediaResult stopDataSource();
    bool        isLiveDataSource();

    typedef enum {
      SOURCE_INVALID = 0,
      SOURCE_FILE = 1,
      SOURCE_HLS = 2,
      SOURCE_HTTP = 3,
    } DATA_SOURCE_TYPE;

    DATA_SOURCE_TYPE getDataSourceType() { return data_source_type_; }

    bool isUnderrun();

    void setOmxTunnelMode(OMX_TUNNEL_MODE mode) { omx_tunnel_mode_ = mode; }
    void setNativeWindow(sp<ANativeWindow> window) { mNativeWindow_ = window; }


  private:

#ifdef ENABLE_RESOURCE_MANAGER
    MediaResult RequestResource();
    MediaResult ReleaseResource();
    bool gotAllResource();
#endif

    void initVariables();
    void recycleResource();

    bool changeOmxCompsState(OMX_STATETYPE state, bool without_clock);
    bool flushOmxComps(bool without_clock);

    bool isDataBuffered();
    bool createDataSource();

    MmpError buildVideoPipeline();
    MmpError buildAudioPipeline();
    MmpError buildSubtitlePipeline();

    MediaResult probeStreamInfo();

    char *url_;
    KeyedVector<String8, String8> *headers_;
    int source_fd_;
    uint64_t source_offset_;
    uint64_t source_length_;

    RawDataSource* mRawDataSource_;
    DATA_SOURCE_TYPE data_source_type_;

    AVInputFormat    *mAVInputFormat_;
    RawDataReader* mRawDataReader_;
    bool stream_info_probed_;

    CorePlayerCbTarget *cb_target_;

    bool pending_seek_;
    bool need_update_offset_;
    TimeStamp seek_target_;

    bool b_pending_start_;
    TimeStamp buffering_start_pts_;

#ifdef ENABLE_RESOURCE_MANAGER
    // Resource management
    ResourceClientHandle mResourceClientHandle_;
    // Audio resource.
    ResHandle app_resource_handle_;
    ResHandle audio_decoder_handle_;
    // Video resource.
    ResHandle vpp_resource_handle_;
    ResHandle video_decoder_handle_;
    // Client class for interaction with the resource manager
    sp<ResourceClient> mMrvlMediaPlayerClient_;

    uint32_t audio_channel_;
    uint32_t video_channel_;

    kdCondSignal *mResourceReadySignal_;
#endif

    // PTS patch for HLS: sometimes pts is discontinue during playback.
    int64_t hls_position_offset_;
    int64_t hls_seek_time_;

    // When underrun happens, need to pause until buffering enough data.
    bool b_is_underrun_;
    bool b_underrun_status_;
    bool mVideoRenderingStarted_;

    MEDIA_FILE_INFO file_info;

    typedef enum {
      STATE_IDLE = 0,
      STATE_PLAYING,
      STATE_PAUSED,
      STATE_STOPPED,
    } STATE;
    STATE state_;

    // The video id and stream index.
    int32_t current_video_;     //in normal case 0
    int32_t video_id_;

    // The audio id and stream index.
    int32_t current_audio_; //in normal case 1
    int32_t audio_id_;

    // The subtitle id and stream index.
    int32_t subtitle_id_;

    bool video_complete_;
    bool audio_complete_;

    MHALClock *mMHALClock_;
    MHALAudioDecoder *mMHALAudioDecoder_;
    MHALAudioRenderer *mMHALAudioRenderer_;
    MHALVideoDecoder *mMHALVideoDecoder_;
    vector<MHALOmxComponent *> mMHALOmxComponents_;

    OMX_TUNNEL_MODE omx_tunnel_mode_;
    sp<ANativeWindow> mNativeWindow_;
    AndroidNativeWindowRenderer *mAndroidNativeWindowRenderer_;
    AndroidAudioRenderer *mAndroidAudioRenderer_;
    MmpClock *mMmpClock_;

    vector<MmpElement*> subtitle_engines_;

    // external subtitle count
    int32_t ext_sub_count_;

    bool sending_subtitle_;

    bool prepared_;

    vector<TimedTextDemux *> ext_srt_dmx_list_;

    kdCondSignal *mStateSignal_;
    kdCondSignal *mFlushSignal_;
    uint32_t mStateCount_;
    uint32_t mFlushCount_;

    // Since sometimes we have more than one instance running at the same time, we
    // add a log tag here to distinguish different instance's log.
    char log_tag_[64];
    static volatile int32_t instance_id_;
};

}
#endif
