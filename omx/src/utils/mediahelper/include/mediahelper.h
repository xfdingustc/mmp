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
#ifndef MEDIA_HELPER_H_
#define MEDIA_HELPER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mediahelper_type.h>
#include <assert.h>

/*
mediahelper user guide:
1 Introduction
  It's help to control print some frequence message and dump media data
2 How to use
  You can push the mediaconfig.txt to /data/ and enable or disable what you want to
  For example:
  1> Modify config value
    <1> you can modify mediainfo_dump_path to /data/ you device /mnt/media/usb,
     but it now fail to write to this path. I'll get the reason and fix it.
     So you still need to mkdir /tmp and write to this folder which I tried pass.
    <2> You can enable or disable current option by modify the value after = to 1 or 0.
  2> Add config
    You can add some config yourself then get it in your code.
*/


#define MEDIA_VALUE_LENGTH 24
#ifdef _ANDROID_
#include <cutils/log.h>
#define MEDIA_PRINT(...) \
  do {                                                                  \
    __android_log_print(4, "MediaHelper", __VA_ARGS__); \
  }while(0)
#else
#define MEDIA_PRINT(...) printf(__VA_ARGS__)
#endif

// mediainfo helper property name :
#define MEDIA_DUMP_PATH         "mediainfo_dump_path"
#define DUMP_AUDIO_ES           "mediainfo_dump_audio_es_data"
#define DUMP_AUDIO_CBUF_ES      "mediainfo_dump_audio_cbuf_es_data"
#define DUMP_AUDIO_CBUF_DONE_ES "mediainfo_dump_audio_cbuf_done_es_data"
#define DUMP_AUDIO_FRAME        "mediainfo_dump_audio_frame_data"
#define DUMP_VIDEO_ES           "mediainfo_dump_video_es_data"
#define DUMP_VIDEO_FRAME        "mediainfo_dump_video_frame_data"
#define CLEAR_DUMP              "mediainfo_clear_dump"
#define PRINT_BD_IN             "mediainfo_print_bd_in"
#define PRINT_BD_IN_BACK        "mediainfo_print_bd_in_back"
#define PRINT_BD_OUT            "mediainfo_print_bd_out"
#define PRINT_BD_OUT_BACK       "mediainfo_print_bd_out_back"
#define PRINT_PTS_IN            "mediainfo_print_pts_in"
#define PRINT_PTS_OUT           "mediainfo_print_pts_out"
#define PUSH_BUFFER_LOOP        "mediainfo_pushbufferloop"
#define PUSH_BUFFER_DONE        "mediainfo_pushbufferdone"
#define PUSH_AMPBD              "mediainfo_pushampbd"
#define VIDEO_SECURE_INFO       "mediainfo_print_video_secure_info"
#define AUDIO_SECURE_INFO       "mediainfo_print_audio_secure_info"


typedef enum {
  MEDIA_VIDEO = 0,
  MEDIA_AUDIO,
  MEDIA_SUBTITLE,
}MH_MEDIA_TYPE;

typedef enum {
  MEDIA_DATA_ES = 0,  // es data
  MEDIA_DATA_FRAME,   // audio pcm or video frame
}MH_DATA_TYPE;

typedef enum {
  MEDIA_DUMP_ES = 0,     // dump es data
  MEDIA_DUMP_FRMAE,      // dump the decoded data
  MEDIA_SEEK_CLEAR,      // clear dumped data (include es and frame) when seek
}MH_DUMP_TYPE;

typedef struct MediainfoHelperEntry {
  MEDIA_S8 *key;
  MEDIA_S8 *value;
} MediainfoHelperEntry;

typedef struct MediainfoEntry {
  MEDIA_U32 count;
  MediainfoHelperEntry *elemts;
} MediainfoEntry;

/* @brief dump the data
 * @param type video, audio or subtitle
 * @param data_type es data or decoded data
 * @return 1 success
 *             0 fail
 */
MEDIA_U8 mediainfo_dump_data(MH_MEDIA_TYPE type,
    MH_DUMP_TYPE data_type, MEDIA_U8 *bits, MEDIA_U32 len);

/* @brief judge if the tag is enabled
 * @param type video, audio or subtitle
 * @param data_type es data or decoded data
 * @return 1 the is enabled
 *             0 the doesn't enabled
 */
MEDIA_U8 mediainfo_is_enable_dump(MH_MEDIA_TYPE type,
    MH_DUMP_TYPE data_type);

/* @brief remove the dumped data
 */
void mediainfo_remove_dumped_data(MH_MEDIA_TYPE type);

/* @brief judge if the tag is enabled
 * @param type video, audio or subtitle
 * @param tag the judged tag info
 * @return 1 the tag is enabled
 *             0 the tag doesn't enabled
 */
MEDIA_BOOL mediainfo_is_enable(MH_MEDIA_TYPE type, MEDIA_U8 *tag);

void mediainfo_set_log_callback(void *callback);

void mediainfo_print_info();

void mediainfo_add_tag(MH_MEDIA_TYPE type, MEDIA_U8 *tag,
    MEDIA_BOOL enable);

MEDIA_U8 mediainfo_get_property(const MEDIA_S8 *key, MEDIA_S8 *value);

void mediainfo_set_property(const MEDIA_S8 *key, MEDIA_S8 *value);

MEDIA_U32 mediainfo_buildup();

void mediainfo_teardown();
#endif //MEDIA_HELPER_H_
