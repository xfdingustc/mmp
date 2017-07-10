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

#ifndef ANDROID_MEDIA_DEFS_H
#define ANDROID_MEDIA_DEFS_H

extern "C" {
#include "avcodec.h"
#include "avformat.h"
}

namespace mmp {

#define MAX_SUPPORT_STREAMS 25

typedef uint64_t TimeStamp;

typedef enum {
  kSuccess = 0,              // 0x0000, 0
  kUnexpectedError,          // 0x0001, 1
  kUnsupported,              // 0x0002, 2
  kOutOfMemory,              // 0x0003, 3
  kBadPointer,               // 0x0004, 4
  kInvalidArgument,          // 0x0005, 5
  kSlewRateOutOfRange,       // 0x0006, 6
  kNotifyTargetNotFound,     // 0x0007, 7
  kNoClock,                  // 0x0008, 8
  kSoftwareOnly,             // 0x0009, 9
  kSampleFlushed,            // 0x000a, 10
  kSampleWasPreroll,         // 0x000b, 11
  kEngineNeedsReset,         // 0x000c, 12
  kEngineNeedsSetup,         // 0x000d, 13
  kOutOfResources,           // 0x000e, 14
  kAVOptionTypeMismatch,     // 0x000f, 15
  kDecodeError,              // 0x0010, 16
  kPresentError,             // 0x0011, 17
  kNoMetadata,               // 0x0012, 18
  kNoDataAvailable,          // 0x0013, 19
  kServerDead,               // 0x0014, 20
  kMissingRequiredPTS,       // 0x0015, 21
  kInsufficientHWResources,  // 0x0016, 22
  kInProgress,               // 0x0017, 23
} MediaResult;

typedef struct _MV_STREAM_INFO {
  // stream ES data codec type
  AVCodecID codec_id;

  // stream index in whole source, DR will use the stream index to decide if the pkt is this
  // stream data for example: stream 0 is video, stream 1 is audio, stream 2 is subtitle
  uint32_t stream_index;

  // the stream start time, usually useful for ts files with multiple programs
  TimeStamp start_time;

  // the stream duration
  TimeStamp duration;

  AVStream *avstream;
} MV_STREAM_INFO;


typedef enum {
  FILE_TYPE_NONE,
  FILE_TYPE_MATROSKA,
  FILE_TYPE_MP3,
  FILE_TYPE_WMA,
  FILE_TYPE_AAC,
  FILE_TYPE_WAV,
  FILE_TYPE_FLV,
  FILE_TYPE_REAL,
  FILE_TYPE_MP4,
  FILE_TYPE_MPEG,
  FILE_TYPE_AVI,
  FILE_TYPE_TS,
  FILE_TYPE_ASF,
  FILE_TYPE_RAW_PCM,
  FILE_TYPE_FLAC,
} FILE_TYPE;



typedef struct _MEDIA_FILE_INFO_ {
  MV_STREAM_INFO audio_stream_info[MAX_SUPPORT_STREAMS];
  MV_STREAM_INFO subtitle_stream_info[MAX_SUPPORT_STREAMS];
  MV_STREAM_INFO video_stream_info[MAX_SUPPORT_STREAMS];
  uint32_t total_video_counts;
  uint32_t total_subtitle_counts;
  uint32_t total_audio_counts;
  uint32_t total_data_counts;

  uint32_t supported_video_counts;
  uint32_t supported_audio_counts;
  uint32_t supported_subtitle_counts;

  uint32_t bit_rate;
  uint64_t file_size;
  TimeStamp duration;
  TimeStamp start_time;

} MEDIA_FILE_INFO;

extern const char *MMP_MIMETYPE_VIDEO_VPX;
extern const char *MMP_MIMETYPE_VIDEO_AVC;
extern const char *MMP_MIMETYPE_VIDEO_MPEG4;
extern const char *MMP_MIMETYPE_VIDEO_H263;
extern const char *MMP_MIMETYPE_VIDEO_MPEG2;
extern const char *MMP_MIMETYPE_VIDEO_RAW;
extern const char *MMP_MIMETYPE_VIDEO_MSMPEG4;
extern const char *MMP_MIMETYPE_VIDEO_MJPEG;
extern const char *MMP_MIMETYPE_VIDEO_SORENSON_SPARK;
extern const char *MMP_MIMETYPE_VIDEO_WMV;
extern const char *MMP_MIMETYPE_VIDEO_VC1;
extern const char *MMP_MIMETYPE_VIDEO_VP6;
extern const char *MMP_MIMETYPE_VIDEO_VP6F;
extern const char *MMP_MIMETYPE_VIDEO_RV;

extern const char *MMP_MIMETYPE_AUDIO_AMR_NB;
extern const char *MMP_MIMETYPE_AUDIO_AMR_WB;
extern const char *MMP_MIMETYPE_AUDIO_MPEG;           // layer III
extern const char *MMP_MIMETYPE_AUDIO_MPEG_LAYER_I;
extern const char *MMP_MIMETYPE_AUDIO_MPEG_LAYER_II;
extern const char *MMP_MIMETYPE_AUDIO_AAC;
extern const char *MMP_MIMETYPE_AUDIO_QCELP;
extern const char *MMP_MIMETYPE_AUDIO_VORBIS;
extern const char *MMP_MIMETYPE_AUDIO_G711_ALAW;
extern const char *MMP_MIMETYPE_AUDIO_G711_MLAW;
extern const char *MMP_MIMETYPE_AUDIO_RAW;
extern const char *MMP_MIMETYPE_AUDIO_FLAC;
extern const char *MMP_MIMETYPE_AUDIO_AAC_ADTS;
extern const char *MMP_MIMETYPE_AUDIO_AC3;
extern const char *MMP_MIMETYPE_AUDIO_EAC3;
extern const char *MMP_MIMETYPE_AUDIO_DTS;
extern const char *MMP_MIMETYPE_AUDIO_WMA;
extern const char *MMP_MIMETYPE_AUDIO_RA;

}

#endif
