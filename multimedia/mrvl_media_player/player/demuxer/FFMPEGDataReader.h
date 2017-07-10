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

#ifndef ANDROID_FFMPEGDATAREADER_H
#define ANDROID_FFMPEGDATAREADER_H

extern "C" {
#include <stdio.h>
#include <sys/prctl.h>

#include "avutil.h"
#include "avcodec.h"
#include "avformat.h"
#include "metadata.h"
#include "avio.h"
}

#include <list>
#include "condSignal.h"
#include "formatters/MRVLFormatter.h"
#include "RawDataReader.h"

using namespace std;

namespace mmp {

typedef enum {
  TS_DR_IDLE = 0,
  TS_DR_PLAY,
  TS_DR_PAUSE,
  TS_DR_RESUME,
  TS_DR_STOP,
  TS_DR_SEEK,
  TS_DR_EXIT,
  TS_DR_INVALID,
} DR_STATE, DR_COMMAND;

typedef struct  {
  MmpPad* pad;

  AVStream* avstream;
} FFStream;


class FFMPEGDataReader;

class FFMPEGDemuxTask : public MmuTask {
  public:
    explicit              FFMPEGDemuxTask(FFMPEGDataReader* demux);
                          ~FFMPEGDemuxTask() {};
    virtual   mbool       ThreadLoop();
  private:
    FFMPEGDataReader*     demux_;
};


class FFMPEGDataReader : public RawDataReader {
  friend class FFMPEGDemuxTask;
  public:
    explicit FFMPEGDataReader(MRVLDataReaderCbTarget* cb_target);
    virtual ~FFMPEGDataReader();

    MediaResult Probe(const char *url, MEDIA_FILE_INFO *file_info);
    MediaResult Probe(const char *url, void *pb, void *fmt, MEDIA_FILE_INFO *pMediaInfo);
    MediaResult Init();

    MediaResult Start(double speed);

    MediaResult Stop();
    MediaResult Seek(uint64_t seek_pos_us);

    MediaResult FlushDataReader();

    MediaResult SetCurrentStream(int32_t video_idx, int32_t audio_idx);

    bool isDataReaderEOS() { return b_datareader_eos_; }
    TimeStamp getLatestPts() { return last_seen_pts_; }


  private:
    MediaResult ProbeStream(MEDIA_FILE_INFO *pMediaInfo);
    MediaResult CheckDuration(MEDIA_FILE_INFO *pMediaInfo);
    MediaResult CloseFFmpeg();

    const mchar* getMimeType(AVCodecID codec_id);

    MediaResult stream_seek(int64_t timestamp, int32_t flags);
    MediaResult stream_seek_by_byte(int64_t timestamp);
    MediaResult stream_seek_by_frame(int64_t timestamp, int32_t flags);

    bool calculatePTS(MmpBuffer *pAVPkt, AVPacket *pkt);
    bool applyPtsPatches(MmpBuffer *pkt);
    bool updateVideoFrameRate(int32_t stream_idx);

    bool needFormat(AVCodecID codec_id);
    bool sendEOSBuffer();

    bool setRAProperty(MmpCaps &caps, AVStream *av_stream);
    bool setPCMProperty(MmpCaps &caps, AVCodecID codec_id);
    bool setWMAProperty(MmpCaps &caps, AVCodecContext *codec_ctx);

    void* mainTaskEntry();

    MmpError  onPlay();
    MmpError  onStop();
    MmpError  onSeek(MmpClockTime seek_time);
    MmpError  onExit();

    class ReaderCommand {
      public:
        ReaderCommand(DR_COMMAND cmd, uint64_t param = 0)
          : mCommand(cmd),
            mParam(param) {
        }

        ~ReaderCommand() {}

        DR_COMMAND mCommand;
        kdCondSignal mCompleteSignal;
        uint64_t mParam;
    };

    bool hasPendingCmd();

    MRVLDataReaderCbTarget* cb_target_;

    KDThreadMutex *mMsgQLock_;
    list<ReaderCommand*> mMsgQ_;

    // Media Info
    MEDIA_FILE_INFO *pMediaInfo_;

    // I frame checking
    uint32_t frames_without_I_frames_;
    bool check_I_frames_;

    TimeStamp start_time_;
    TimeStamp duration_;

    // PTS validation
    bool b_pts_reliable_[MAX_SUPPORT_STREAMS];
    int64_t last_pts_[MAX_SUPPORT_STREAMS];
    // DTS validation
    bool b_dts_reliable_[MAX_SUPPORT_STREAMS];
    int64_t last_dts_[MAX_SUPPORT_STREAMS];

    bool b_first_key_frame_after_seek_;

    // Runtime information
    int32_t current_video_;
    int32_t current_audio_;
    int32_t current_subtitle_;
    bool b_seek_by_bytes_;

    // FFMPEG context
    AVFormatContext *mAVFormatContext_;

    MRVLFormatter *mMRVLFormatter_;

    bool b_pending_seek_;

    // PTS patch 1: detect pts discontinue.
    // TODO: audio pts discontinue should be detect separately.
    static const TimeStamp pts_delta_torl_ = 2 * 1000 * 1000; // max pts delta is 2s
    TimeStamp video_pts_offset_;
    TimeStamp last_video_pts_;

    // PTS patch 2: if ffmpeg doesn't give pts, set pts according to framerate.
    TimeStamp video_pts_delta_; //video PTS delta in us. 1000000 * 1 / framerate

    // PTS patch 3: detect underrun.
    TimeStamp last_seen_pts_;
    bool b_datareader_eos_;

    // PTS patch 4: get seek time after seek by max(audio pts, video pts).
    bool b_getting_audio_pts_after_seek_;
    TimeStamp audio_pts_after_seek_;
    bool b_getting_video_pts_after_seek_;
    TimeStamp video_pts_after_seek_;

    // sub streams
    FFStream sub_streams_[MAX_SUPPORT_STREAMS];

    FFMPEGDemuxTask* main_task_;
    DR_STATE dr_state_;
};
}
#endif

