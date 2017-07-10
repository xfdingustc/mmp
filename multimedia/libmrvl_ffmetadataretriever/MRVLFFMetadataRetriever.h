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

#ifndef _MRVL_FFMPEG_METADATA_RETRIEVER_H_
#define _MRVL_FFMPEG_METADATA_RETRIEVER_H_

#include "FileDataSource.h"

#include <utils/KeyedVector.h>
#include <StagefrightMetadataRetriever.h>

namespace android {

#define RETRIEVER_LOGV(fmt, ...) \
    ALOGV("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define RETRIEVER_LOGD(fmt, ...) \
    ALOGD("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define RETRIEVER_LOGI(fmt, ...) \
    ALOGI("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define RETRIEVER_LOGW(fmt, ...) \
    ALOGW("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define RETRIEVER_LOGE(fmt, ...) \
    ALOGE("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)


class MRVLFFMetadataRetriever : public StagefrightMetadataRetriever {
  public:
    MRVLFFMetadataRetriever();
    ~MRVLFFMetadataRetriever();

    status_t setDataSource(const char *url, const KeyedVector<String8, String8> *headers = NULL);
    status_t setDataSource(int fd, int64_t offset, int64_t length);
    VideoFrame* getFrameAtTime(int64_t timeUs, int option);
    MediaAlbumArt* extractAlbumArt();
    const char* extractMetadata(int keyCode);

  private:
    void reset();

    bool is_metadata_extracted_;
    KeyedVector<int, String8> metadata_;

    bool init_;
    bool use_stagefright_;
    char *source_url_;

    AVFormatContext* context_;
    RawDataSource* mRawDataSource_;

    static Mutex lock_;
};

}
#endif   /*_MRVL_FFMPEG_METADATA_RETRIEVER_H_*/
