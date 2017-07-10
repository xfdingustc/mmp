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

#ifndef MMP_TIMED_TEXT_DEMUX_H
#define MMP_TIMED_TEXT_DEMUX_H

#include <media/stagefright/FileSource.h>

#include "frameworks/mmp_frameworks.h"
#include "mmu_mstring.h"

using namespace android;
using namespace mmu;

namespace mmp {

class TimedTextDemux;

class TimeTextDemuxTask : public MmuTask {
  public:
    explicit              TimeTextDemuxTask(TimedTextDemux* demux);
                          ~TimeTextDemuxTask() {};
    virtual   mbool       ThreadLoop();
  private:
    TimedTextDemux*       demux_;

};

class TimedTextDemux : public MmpElement {
  friend class TimeTextDemuxTask;
  friend class TimedTextSinkPad;
  public:
    explicit TimedTextDemux(
        int32_t fd, uint64_t offset, uint64_t length);
    virtual ~TimedTextDemux();
    void start();
    void stop();
    MmpError seek(uint64_t seek_pos_us);

  private:
    MmpError  scanSource();
    MmpError  pullAndPush();
    MmpError getText(AString *text, MmpClockTime* startTimeUs, MmpClockTime* endTimeUs);

  private:
    TimeTextDemuxTask*    main_task_;
    MmpPad*   text_src_pad_;

    struct TextInfo {
      MmpClockTime startTimeUs;
      MmpClockTime endTimeUs;
      // The offset of the text in the original file.
      off64_t offset;
      int textLen;
    };

    int32_t index_;
    vector<TextInfo> text_vector_;

    FileSource *source_;

    MmpError getNextSubtitleInfo(moffset *offset, TextInfo *info);
    MmpError readNextLine(moffset *offset, MString *data);

};

}

#endif
