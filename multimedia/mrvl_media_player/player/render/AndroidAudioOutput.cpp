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


#include "AndroidAudioOutput.h"
#include <utils/Log.h>

#undef  LOG_TAG
#define LOG_TAG "AndroidAudioRenderer"

namespace mmp {

MmpError AudioRenderSinkPad::ChainFunction(MmpBuffer* buf) {
  AndroidAudioRenderer* render =
        reinterpret_cast<AndroidAudioRenderer*>(GetOwner());
    M_ASSERT_FATAL(render, MMP_PAD_FLOW_ERROR);

    render->render(buf);
  return MMP_NO_ERROR;
}

MmpError AudioRenderSinkPad::LinkFunction() {
  M_ASSERT_FATAL(peer_, MMP_PAD_FLOW_NOT_LINKED);

  MmpCaps &peer_caps = peer_->GetCaps();

  MmpCaps &my_caps = GetCaps();

  // Call operator= to copy vaule.
  my_caps = peer_caps;

  return MMP_NO_ERROR;
}

AndroidAudioRenderer::AndroidAudioRenderer()
  : mAudioTrack_(NULL),
    mNumFramesPlayed_(0),
    MmpElement("audio_render") {
  // Create Mmp Pads
  MmpPad* sink_pad = static_cast<MmpPad*>(new AudioRenderSinkPad("audio_sink",
      MmpPad::MMP_PAD_SINK, MmpPad::MMP_PAD_MODE_NONE));
  AddPad(sink_pad);
}

AndroidAudioRenderer::~AndroidAudioRenderer() {
  ALOGD("Begin destroy AndroidAudioRenderer.");
  ALOGD("AndroidAudioRenderer is destroyed.");
}

void AndroidAudioRenderer::render(MmpBuffer* buf) {
  if (!buf) {
    ALOGE("buf is NULL!");
  }

  if (mAudioTrack_ == NULL) {
    MmpPad* sink_pad = GetSinkPad();
    MmpCaps &caps = sink_pad->GetCaps();
    mint32 sample_rate = 0, channels = 0;
    caps.GetInt32("sample_rate", &sample_rate);
    caps.GetInt32("channels", &channels);
    ALOGD("sample_rate = %d, channels = %d", sample_rate, channels);

    audio_channel_mask_t audioMask = audio_channel_out_mask_from_count(channels);
    mAudioTrack_ = new AudioTrack(AUDIO_STREAM_MUSIC, sample_rate, AUDIO_FORMAT_PCM_16_BIT,
        audioMask, 1024, AUDIO_OUTPUT_FLAG_NONE, NULL, NULL, 0);
    if ((mAudioTrack_->initCheck()) != OK) {
      ALOGE("Failed to create AudioTrack!");
    }
    ALOGD("mAudioTrack_->frameSize() = %d", mAudioTrack_->frameSize());
    ALOGD("mAudioTrack_->latency() = %dms", mAudioTrack_->latency());
    mAudioTrack_->start();
  }

  int bytes_written = 0;
  int bytes = 0;
  if (mAudioTrack_ != NULL) {
    while (buf->size > bytes_written) {
      bytes = mAudioTrack_->write(buf->data + bytes_written, buf->size - bytes_written);
      bytes_written += bytes;
      mNumFramesPlayed_ += bytes_written / mAudioTrack_->frameSize();
    }
  }
  ALOGD("bytes_written = %d, frames played = %lld", bytes_written, mNumFramesPlayed_);

  MmpPad* pad = GetSinkPad();
  MmpEvent render_event(MmpEvent::MMP_EVENT_BUFFER_RENDERED, buf);
  pad->PushEvent(&render_event);
}

}

