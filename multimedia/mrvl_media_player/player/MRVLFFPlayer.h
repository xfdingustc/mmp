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

#ifndef _MRVL_FFMPEG_PLAYER_H_
#define _MRVL_FFMPEG_PLAYER_H_

#include "CorePlayer.h"
#include "MediaPlayerOnlineDebug.h"
#include "thread_utils.h"
#include "mmu_thread.h"

namespace mmp {

class MRVLFFPlayerCbTarget {
  public:
    virtual ~MRVLFFPlayerCbTarget() {};
    virtual void sendMsg(int msg, int ext1 = 0, int ext2 = 0, void * parcel=NULL) = 0;
};


class MRVLFFPlayer :  virtual public CorePlayerCbTarget {
  public:
                        MRVLFFPlayer(MRVLFFPlayerCbTarget *target);
    virtual             ~MRVLFFPlayer();

    status_t            setDataSource(const char *url);
#ifdef ANDROID
    status_t            setDataSource(const char *url,
                            const KeyedVector<String8, String8> *headers);
#endif
    status_t            setDataSource(int fd, int64_t offset, int64_t length);

    status_t            setSecondDataSource(const char* url, const char* mime_type);
    status_t            setSecondDataSource(int fd, int64_t offset, int64_t length,
                                            const char* mime_type);

    status_t            prepare();
    status_t            prepareAsync();
    status_t            start();
    status_t            stop();
    status_t            pause();
    status_t            seekTo(int msec);
    status_t            reset();
    bool                setLooping(bool loop);
    bool                getLooping();
    status_t            getCurrentPosition(int *msec);
    status_t            getDuration(int *msec);
    bool                isPlaying();
    status_t            setVolume(float leftVolume, float rightVolume);

    status_t            fastforward(int speed);
    status_t            slowforward(int speed);
    status_t            fastbackward(int speed);

    uint32_t            GetVideoSubStreamCounts();

    uint32_t            GetAudioSubStreamCounts();
    int32_t             GetCurrentAudioSubStream();
    bool                getAudioStreamsDescription(streamDescriptorList *desc_list);
    status_t            SetCurrentAudioSubStream(int sub_stream_id);

    uint32_t            GetSubtitleSubStreamCounts();
    int32_t             GetCurrentSubtitleSubStream();
    bool                getSubtitleStreamsDescription(streamDescriptorList *desc_list);
    status_t            SetCurrentSubtitleSubStream(int sub_stream_id);
    status_t            DeselectSubtitleSubStream(int subid) ;

    void setOmxTunnelMode(OMX_TUNNEL_MODE mode) {
      if (core_player_) {
        core_player_->setOmxTunnelMode(mode);
      }
    }
#ifdef _ANDROID_
    void setNativeWindow(sp<ANativeWindow> window) {
      if (core_player_) {
        core_player_->setNativeWindow(window);
      }
    }
#endif
    bool                needCheckTimeOut();

    status_t            sendMsg(MSG_TYPE message, void* data);

    void                *MRVLFFPlayerWorkThread();
    static void         *staticWorkThreadEntry(void* thiz);


  private:
    enum State {
      State_Invalid = -1,
      State_WaitingForPrepare = 0,
      State_Preparing,
      State_Stopped,
      State_Paused,
      State_Playing,
      State_Shutdown
    };

    const char* State2Str(State state) {
      switch (state) {
        STRINGIFY(State_WaitingForPrepare);
        STRINGIFY(State_Preparing);
        STRINGIFY(State_Stopped);
        STRINGIFY(State_Paused);
        STRINGIFY(State_Playing);
        STRINGIFY(State_Shutdown);
        default: return "unknown";
      }
    };

    void  setNextState_l(State state);
    State getNextState();
    bool  clearAllStateRequests();
    bool  waitingForWork(int msecTimeout);

    bool transferFromPreparing();
    bool transferFromStopped();
    bool transferFromPaused();
    bool transferFromPlaying();
    bool processSeek();
    bool processTrickPlay();

    State cur_state_;
    State tgt_state_;

#define QUEUE_LEN 32
    State state_queue_[QUEUE_LEN];
    int   front_;
    int   end_;

    double cur_play_speed_;
    double tgt_play_speed_;

    CorePlayer *core_player_;

    MmuMutex  action_mutex_;
    MmuCondition mCondition_;
    PipeEvent wakeup_event_;
    pthread_t work_thread_id_;

    bool pending_seek_;
    int  pending_seek_target_;
    bool seek_in_progress_;

    MRVLFFPlayerCbTarget *cb_target_;

    bool is_loopback_enabled_;
    bool is_eos_sent_;

    // Since sometimes we have more than one instance running at the same time, we
    // add a log tag here to distinguish different instance's log.
    char log_tag_[64];
    static volatile int32_t instance_id_;
};

}
#endif   /*_MRVL_FFMPEG_PLAYER_H_*/
