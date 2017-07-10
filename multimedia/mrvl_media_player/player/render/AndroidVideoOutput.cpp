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

#include "AndroidVideoOutput.h"
#include <utils/Log.h>

#undef  LOG_TAG
#define LOG_TAG "AndroidNativeWindowRenderer"

namespace mmp {

MmpError VideoRenderSinkPad::ChainFunction(MmpBuffer* buf) {
  AndroidNativeWindowRenderer* render =
      reinterpret_cast<AndroidNativeWindowRenderer*>(GetOwner());
  M_ASSERT_FATAL(render, MMP_PAD_FLOW_ERROR);

  render->receiveFrame(buf);

  return MMP_NO_ERROR;
}

AndroidNativeWindowRenderer::AndroidNativeWindowRenderer(
    const sp<ANativeWindow> &nativeWindow)
  : mNativeWindow_(nativeWindow),
    MmpElement("video_render") {
  // Create Mmp Pads
  MmpPad* sink_pad = static_cast<MmpPad*>(new VideoRenderSinkPad("video_sink",
      MmpPad::MMP_PAD_SINK, MmpPad::MMP_PAD_MODE_NONE));
  AddPad(sink_pad);

  // Start working thread.
  Run();
}

AndroidNativeWindowRenderer::~AndroidNativeWindowRenderer() {
  RequestExit();
  Join();
}

void AndroidNativeWindowRenderer::receiveFrame(MmpBuffer* buf) {
  MmuAutoLock lock(queue_mutex_);
  mVideoQueue_.push_back(buf);
  ALOGD("%d frames are waiting to show.", mVideoQueue_.size());
  // TODO: should wake up thread immediately.
}

void AndroidNativeWindowRenderer::render(MmpBuffer* buf) {
  if (buf) {
    status_t err = mNativeWindow_->queueBuffer(
        mNativeWindow_.get(), (GraphicBuffer*)(buf->data), -1);
    if (err != 0) {
      ALOGE("queueBuffer %p failed with error %s (%d)", buf->data, strerror(-err), -err);
      return;
    }

    ALOGD("queued one GraphicBuffer %p to native window", buf->data);
    MmpPad* pad = GetSinkPad();
    MmpEvent render_event(MmpEvent::MMP_EVENT_BUFFER_RENDERED, buf);
    pad->PushEvent(&render_event);
  }
}

mbool AndroidNativeWindowRenderer::ThreadLoop() {
  const int sleepTimeUs = 10 * 1000; // 10ms
  const MmpClockTime kEarlyThresholdUs = 10 * 1000;
  const MmpClockTime kLateThresholdUs = 10 * 1000;

  bool will_sleep = true;
  int wait_time = sleepTimeUs;

  {
    MmuAutoLock lock(queue_mutex_);

    MmpClock *clock = GetClock();
    if (!clock) {
      ALOGE("clock is not found!");
      return false;
    }
    MmpClockTime media_time_us = clock->GetTime();
    if (!mVideoQueue_.empty()) {
      MmpBuffer* buf = *(mVideoQueue_.begin());
      if (buf->pts < media_time_us - kEarlyThresholdUs) {
        // Out of date, quickly push.
        render(buf);
        mVideoQueue_.pop_front();
        will_sleep = false;
      } else if (buf->pts > media_time_us + kLateThresholdUs) {
        // Too early, sleep to wait its time to come.
        will_sleep = true;
        wait_time = buf->pts - (media_time_us + kLateThresholdUs);
      } else {
        // It is proper to render it.
        render(buf);
        will_sleep = false;
        mVideoQueue_.pop_front();
      }
    } else {
      // Nothing to do.
      will_sleep = true;
    }
  }

  if (will_sleep) {
    // At least wait 10ms so that CPU will not be too busy.
    usleep((wait_time > sleepTimeUs) ? wait_time : sleepTimeUs);
  }
  return true;
}

}
