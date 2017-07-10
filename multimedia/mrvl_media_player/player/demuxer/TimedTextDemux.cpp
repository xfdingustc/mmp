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

#include <sys/types.h>
#include <unistd.h>

#include <media/stagefright/foundation/AString.h>

#include "TimedTextDemux.h"

#undef  LOG_TAG
#define LOG_TAG "TimedTextDemux"

namespace mmp {

TimeTextDemuxTask::TimeTextDemuxTask(TimedTextDemux* demux)
  : demux_(demux) {
}

mbool TimeTextDemuxTask::ThreadLoop() {
  demux_->pullAndPush();
  usleep(100 * 1000);
  return true;
}

TimedTextDemux::TimedTextDemux(
    int32_t fd, uint64_t offset, uint64_t length)
  : MmpElement("timed-text-demux"){
  ALOGD("Enter %s() line %d", __FUNCTION__, __LINE__);

  index_ = 0;
  source_ = new FileSource(fd, offset, length);

  text_src_pad_ = new MmpPad("ext-sub", MmpPad::MMP_PAD_SRC, MmpPad::MMP_PAD_MODE_PUSH);
  AddPad(text_src_pad_);

  main_task_ = new TimeTextDemuxTask(this);

  scanSource();
}

TimedTextDemux::~TimedTextDemux() {
  if (main_task_) {
    main_task_->RequestExit();
    main_task_->Join();

    delete main_task_;
  }
}

void TimedTextDemux::start() {
  if (!main_task_) {
    main_task_ = new TimeTextDemuxTask(this);
  }
  if (main_task_) {
    main_task_->Run();
  }
}

void TimedTextDemux::stop() {
  if (main_task_) {
    main_task_->RequestExit();
    main_task_->Join();

    delete main_task_;
    main_task_ = NULL;
  }
}

MmpError TimedTextDemux::seek(uint64_t seek_pos_us) {
  // TODO: make seek more accurate!
  // Read all subtitle again!
  index_ = 0;
  return MMP_NO_ERROR;
}

MmpError TimedTextDemux::pullAndPush() {
  MmpClockTime startTimeUs = 0;
  MmpClockTime endTimeUs = 0;
  AString *text = new AString();

  MmpError ret = getText(text, &startTimeUs, &endTimeUs);

  if (ret == MMP_ERROR_IO) {
    return MMP_ERROR_IO;
  }
  if (ret == MMP_END_OF_STREAM) {
    return MMP_END_OF_STREAM;
  }

  text->trim();
  const char *data = text->c_str();
  int len = text->size();



  MmpBuffer* buf = new MmpBuffer;
  buf->pts = startTimeUs;
  buf->end_pts = endTimeUs;
  buf->size = len;
  buf->data = (uint8_t *)malloc(len + 1);
  if (!buf) {
    ALOGE("failed in malloc(%d) for TimedTextDemux", len);
    return MMP_UNEXPECTED_ERROR;
  }
  memset(buf->data, 0x00, len + 1);
  memcpy(buf->data, data, len);

  text_src_pad_->Push(buf);
  ALOGD("text=%s, startTimeUs=%lld, endTimeUs=%lld", data, startTimeUs, endTimeUs);

  return MMP_NO_ERROR;
}

MmpError TimedTextDemux::scanSource() {
  muint64 offset = 0;
  mbool endOfFile = false;

  while (!endOfFile) {
    TextInfo info;
    MmpError err = getNextSubtitleInfo(&offset, &info);
    switch (err) {
      case MMP_NO_ERROR:
        text_vector_.push_back(info);
        break;
      case MMP_END_OF_STREAM:
        endOfFile = true;
        break;
      default:
        return err;
    }
  }
  if (text_vector_.empty()) {
    return MMP_ERROR_MALFORMED;
  }
  return MMP_NO_ERROR;
}


MmpError TimedTextDemux::getNextSubtitleInfo(moffset *offset, TextInfo *info) {
  MString data;
  MmpError ret;
  // To skip blank lines
  do {
    if ((ret = readNextLine(offset, &data)) != MMP_NO_ERROR) {
      return ret;
    }
    data.Trim();
  } while(data.Empty());

  // Just ignore the first non-blank line which is subtitle sequence number.
  if ((ret = readNextLine(offset, &data)) != MMP_NO_ERROR) {
    return ret;
  }

  mint32 hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
  // the start time format is: hours:minutes:seconds,milliseconds
  // 00:00:24,600 --> 00:00:27,800
  if (sscanf(data.GetString(), "%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d",
             &hour1, &min1, &sec1, &msec1, &hour2, &min2, &sec2, &msec2) != 8) {
    return MMP_ERROR_MALFORMED;
  }

  info->startTimeUs = ((hour1 * 3600 + min1 * 60 + sec1) * 1000 + msec1) * 1000ll;
  info->endTimeUs = ((hour2 * 3600 + min2 * 60 + sec2) * 1000 + msec2) * 1000ll;
  if (info->endTimeUs <= info->startTimeUs) {
    return MMP_ERROR_MALFORMED;
  }

  info->offset = *offset;
  bool needMoreData = true;
  while (needMoreData) {
    if ((ret = readNextLine(offset, &data)) != MMP_NO_ERROR) {
      if (ret == MMP_END_OF_STREAM) {
        break;
      } else {
        return ret;
      }
    }

    data.Trim();
    if (data.Empty()) {
      // it's an empty line used to separate two subtitles
      needMoreData = false;
    }
  }
  info->textLen = *offset - info->offset;
  return MMP_NO_ERROR;
}

MmpError TimedTextDemux::readNextLine(moffset *offset, MString *data) {
  data->Clear();
  while (true) {
    MmpError error;
    mchar character;
    ssize_t ret;

    ret = source_->readAt(*offset, &character, 1);
    if (ret < 1) {
      if (ret == 0) {
        return MMP_END_OF_STREAM;
      }
      return MMP_UNEXPECTED_ERROR;
    }

    (*offset)++;

    // a line could end with CR, LF or CR + LF
    if (character == 10) {
      break;
    } else if (character == 13) {
      ret = source_->readAt(*offset, &character, 1);
      if (ret < 1) {
        if (ret == 0) {
          return MMP_END_OF_STREAM;
        }
        return MMP_UNEXPECTED_ERROR;
      }

      (*offset)++;
      if (character != 10) {
        (*offset)--;
      }
      break;
    }
    data->Append(character);
  }
  return MMP_NO_ERROR;
}

MmpError TimedTextDemux::getText(
    AString *text, MmpClockTime* startTimeUs, MmpClockTime* endTimeUs) {
  if (index_ >= text_vector_.size()) {
    return MMP_END_OF_STREAM;
  }

  TextInfo &info = text_vector_[index_];
  *startTimeUs = info.startTimeUs;
  *endTimeUs = info.endTimeUs;
  index_++;

  text->clear();

  char *str = new char[info.textLen];
  if (source_->readAt(info.offset, str, info.textLen) < info.textLen) {
        delete[] str;
        return MMP_ERROR_IO;
  }
  text->append(str, info.textLen);
  delete[] str;
  return MMP_NO_ERROR;
}
}
