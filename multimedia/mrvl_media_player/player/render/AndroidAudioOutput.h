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

#ifndef ANDROID_AUDIO_OUTPUT_H_
#define ANDROID_AUDIO_OUTPUT_H_

#include "frameworks/mmp_frameworks.h"
#include <AudioTrack.h>

using namespace android;

namespace mmp {

class AudioRenderSinkPad : public MmpPad {
  public:
    AudioRenderSinkPad(mchar* name, MmpPadDirection dir, MmpPadMode mode)
      : MmpPad(name, dir, mode) {
    }
    ~AudioRenderSinkPad() {};
    virtual MmpError ChainFunction(MmpBuffer* buf);
    virtual MmpError LinkFunction();
};

class AndroidAudioRenderer : public MmpElement {
  public:
    AndroidAudioRenderer();
    virtual ~AndroidAudioRenderer();

    void render(MmpBuffer* buf);

  private:
    sp<AudioTrack> mAudioTrack_;

    int64_t mNumFramesPlayed_;
};

}

#endif
