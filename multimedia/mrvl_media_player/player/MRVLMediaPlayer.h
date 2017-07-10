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

#ifndef ANDROID_MRVLMEDIAPLAYER_H
#define ANDROID_MRVLMEDIAPLAYER_H

#include <utils/Errors.h>
#include <utils/String8.h>
#if PLATFORM_SDK_VERSION < 18
#include <gui/ISurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#else
#include <gui/IGraphicBufferProducer.h>
#endif
#include <gui/Surface.h>

#include <media/MediaPlayerInterface.h>

#include "MRVLFFPlayer.h"

using namespace mmp;
using namespace mmu;

namespace android {

// This definition should be the same as the one in system/core/include/system/audio.h
typedef int audio_io_handle_t;

// MRVLMediaPlayer
// This is the wrapper layer to integrate mrvl media player into android media
// player service. To do that, we change
//  .../frameworks/base/media/libmediaplayerservice/MediaPlayerService.cpp
// to load mrvl media player or other player in android media frameworks.
//
class MRVLMediaPlayer : public MediaPlayerHWInterface,
                        public MRVLFFPlayerCbTarget {
  public:
    MRVLMediaPlayer();
    virtual             ~MRVLMediaPlayer();

    virtual status_t    initCheck();

    virtual status_t    setDataSource(
                            const char *url,
                            const KeyedVector<String8, String8> *headers = NULL);
    virtual status_t    setDataSource(int fd, int64_t offset, int64_t length);

#if PLATFORM_SDK_VERSION < 18
    virtual status_t    setVideoSurfaceTexture(const sp<ISurfaceTexture>& bufferProducer);
#else
    virtual status_t    setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer);
#endif
    virtual status_t    prepare();
    virtual status_t    prepareAsync();
    virtual status_t    start();
    virtual status_t    stop();
    virtual status_t    pause();
    virtual bool        isPlaying();
    virtual status_t    seekTo(int msec);
    virtual status_t    getCurrentPosition(int *msec);
    virtual status_t    getDuration(int *msec);
    virtual status_t    reset();
    virtual status_t    setLooping(int loop);
    // player type should keep sync with MediaPlayerInterface.h
    // However in order that MMP can pass build under various Android version,
    // we just put the value instead of enum here.
    virtual player_type playerType() { return (player_type)6; }
    virtual status_t    setParameter(int key, const Parcel &request);
    virtual status_t    getParameter(int key, Parcel *reply);

    // Invoke a generic method on the player by using opaque parcels
    // for the request and reply.
    //
    // @param request Parcel that is positioned at the start of the
    //                data sent by the java layer.
    // @param[out] reply Parcel to hold the reply data. Cannot be null.
    // @return OK if the call was successful.
    enum {
      PARA_FAST_FORWARD   = 0x1001,
      PARA_FAST_BACKWARD  = 0x1002,
      PARA_SLOW_FORWARD   = 0x1003,

      INVOKE_ID_ENUM_AUDIO_SUBSTREAMS       = 0x2094,
      INVOKE_ID_GET_CURRENT_AUDIO_SUBSTREAM = 0x2095,
      INVOKE_ID_SET_CURRENT_AUDIO_SUBSTREAM = 0x2096,

      INVOKE_ID_ENUM_SUBTITLE_SUBSTREAMS       = 0x2097,
      INVOKE_ID_GET_CURRENT_SUBTITLE_SUBSTREAM = 0x2098,
      INVOKE_ID_SET_CURRENT_SUBTITLE_SUBSTREAM = 0x2099,
      INVOKE_ID_SUBTITLE_SHOW = 0x20a0,
      INVOKE_ID_SET_SUBTITLE_COLOR = 0x20a1,
    };

    virtual status_t    invoke(const Parcel& request, Parcel *reply);

    virtual status_t    setVolume(float leftVolume, float rightVolume);

    virtual status_t    setAudioStreamType(audio_stream_type_t streamType) { return android::OK; }


    // The Client in the MetadataPlayerService calls this method on
    // the native player to retrieve all or a subset of metadata.
    //
    // @param ids SortedList of metadata ID to be fetch. If empty, all
    //            the known metadata should be returned.
    // @param[inout] records Parcel where the player appends its metadata.
    // @return OK if the call was successful.
    virtual status_t    getMetadata(const media::Metadata::Filter& ids, Parcel *records);

    // It is a virtual function in android-4.4.2_r2 since
    // it is declared as virtual in MediaPlayerInterface.h
    audio_stream_type_t getAudioStreamType() const { return AUDIO_STREAM_MUSIC; }

    bool        getLooping();
    status_t    getVideoWidth(int *w);
    status_t    getVideoHeight(int *h);


  private:
    virtual void        sendMsg(int msg, int ext1, int ext2, void *parcel=NULL);
    virtual status_t    getTrackInfo(Parcel *reply) const;
    //added for video resize
    void flushSurfaceTextureInternal();

    const char*         Msg2Str(int32_t msg) {
      switch (msg) {
        STRINGIFY(MEDIA_NOP);
        STRINGIFY(MEDIA_PREPARED);
        STRINGIFY(MEDIA_PLAYBACK_COMPLETE);
        STRINGIFY(MEDIA_BUFFERING_UPDATE);
        STRINGIFY(MEDIA_SEEK_COMPLETE);
        STRINGIFY(MEDIA_SET_VIDEO_SIZE);
        STRINGIFY(MEDIA_TIMED_TEXT);
        STRINGIFY(MEDIA_ERROR);
        STRINGIFY(MEDIA_INFO);
        default: return "unknown";
      }
    }

    typedef struct TimeoutObject{
      int  token;
      char *name;
      int  timeout_us;
      bool send_kill;
    }TimeoutObject;

    typedef struct TimeoutObjectList {
      TimeoutObject *obj;
      TimeoutObjectList *next;
    }TimeoutObjectList;

    bool registerTimeout(TimeoutObject *obj);
    bool unregisterTimeout(uint32_t token_id);

    void* TimeoutThreadEntry();
    static void* staticTimeoutThread(void* thiz);

    MRVLFFPlayer        *mrvl_player_;

    // We need it because we have to tell audio policy manager that we are playing audio,
    // in order that we can pass CTS test.
    audio_io_handle_t audioflinger_handle_;
    bool is_output_started;

    int32_t video_plane_id_;
    int32_t audio_channel_id_;
    uint32_t width_;
    uint32_t height_;

    TimeoutObjectList *mList_;
    volatile int32_t  token_id_;
    MmuMutex     list_mutex_;
    pthread_t timeout_thread_;
    bool      quit_thread_;

    bool    resource_ready_;
    float   left_vol_;
    float   right_vol_;
    bool    set_vol_delay_;

    //added for video resize
#if PLATFORM_SDK_VERSION < 18
    sp<ISurfaceTexture> bufferProducer_;
#else
    sp<IGraphicBufferProducer> bufferProducer_;
#endif
    MmuMutex bufferProducer_lock_;
    uint32_t surface_id_;

    OMX_TUNNEL_MODE omx_tunnel_mode_;
};

}

#endif // ANDROID_MRVLMEDIAPLAYER_H
