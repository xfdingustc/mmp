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

#ifndef ANDROID_WIDEVINE_DATA_READER_H_
#define ANDROID_WIDEVINE_DATA_READER_H_

#include "RawDataReader.h"

using namespace mmp;

namespace android {

class WidevineDataReader : public RawDataReader {
  public:
    explicit WidevineDataReader(MRVLDataReaderCbTarget* cb_target);
    virtual ~WidevineDataReader() {}

    virtual MediaResult Probe(const char *url, MEDIA_FILE_INFO *file_info);
    virtual MediaResult Probe(const char *url, void *pb, void *fmt, MEDIA_FILE_INFO *pMediaInfo);
    virtual MediaResult Init();
    virtual MediaResult Start(double speed);
    virtual MediaResult Stop();
    virtual MediaResult Seek(uint64_t seek_pos_us);
    virtual MediaResult FlushDataReader();
    virtual MediaResult SetCurrentStream(int32_t video_idx, int32_t audio_idx);
    virtual bool isDataReaderEOS();
    virtual TimeStamp getLatestPts();

  private:
    MRVLDataReaderCbTarget* cb_target_;

    // Media Info
    MEDIA_FILE_INFO *pMediaInfo_;

    TimeStamp start_time_;
    TimeStamp duration_;
};

}
#endif
