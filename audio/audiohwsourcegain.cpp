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

#define LOG_TAG "AudioHwSourceGain"

#include <cutils/log.h>

#ifdef _ANDROID_
#include <IAVSettings.h>
#include <OutputControl.h>
#include <RefBase.h>
#include <SourceControl.h>
#include <source_constants.h>
#endif

#ifdef _ANDROID_
using android::sp;
using marvell::AVSettingValue;
using marvell::IAVSettingObserver;
using marvell::IAVSettings;
using marvell::OutputControl;
using marvell::SourceControl;
#endif

class AudioHwSourceGain : public android::RefBase {
  public:
    AudioHwSourceGain() {
      mSourceControl = NULL;
      mOutputControl = NULL;
      mObserver = NULL;
      mSourceId = -1;
      mPhySourceGain = -1;
    }
    ~AudioHwSourceGain() {
      if (mOutputControl != NULL) {
        mOutputControl->removeObserver(mObserver);
      }
      if ((mSourceControl != NULL) && (-1 != mSourceId)) {
        mSourceControl->exitSource(mSourceId);
      }
    }

  private:
  // Define class that will receive the avsetting change notifications.
  class AVSettingObserver : public marvell::BnAVSettingObserver {
    public:
      AVSettingObserver(AudioHwSourceGain& audiohwsourcegain)
          :mAudioHwSourceGain(audiohwsourcegain){
      }
      void OnSettingUpdate(const char* name, const AVSettingValue& value);
    private:
      AudioHwSourceGain& mAudioHwSourceGain;
  };

  public:
    int32_t initSourceGain();
    int32_t getSouceGain();
    sp<SourceControl> mSourceControl;
    sp<OutputControl> mOutputControl;
    sp<IAVSettingObserver> mObserver;
    int32_t mSourceId;
    int32_t mPhySourceGain;

};

int32_t AudioHwSourceGain::initSourceGain() {
  status_t ret;
  if (mSourceControl == NULL) {
    mSourceControl = new SourceControl();
  }
  AudioHwSourceGain::getSouceGain();
  if (mOutputControl == NULL) {
    mOutputControl = new OutputControl();
    // After being registered , when AVSeting  changes SoureGain value ,
    // we can get it by OnSettingUpdate() at once.
    if (mOutputControl != NULL) {
      mObserver = new AVSettingObserver(*this);
      ret = mOutputControl->registerObserver("mrvl.output", mObserver);
      if (android::OK != ret) {
        ALOGE("rigister Observer failed !!!");
        return -1;
      }
    }
  }
  return mPhySourceGain;
}

int32_t AudioHwSourceGain::getSouceGain() {
  String8 FILE_SOURCE_URI;
  char ktmp[80] ;
  if ( mSourceControl != NULL) {
    sprintf(ktmp, "%s%d", marvell::kAudioRenderSourcePrefix, marvell::AUDIO_SOURCE_NONE_ID);
    FILE_SOURCE_URI = String8(ktmp);
    mSourceControl->getSourceByUri(FILE_SOURCE_URI, &mSourceId);
  } else {
    ALOGE(" mSourceControl is null.");
  }
  if (-1 != mSourceId) {
    mSourceControl->getSourceGain(mSourceId, &mPhySourceGain);
    ALOGD("get sourGain = %0x", mPhySourceGain);
  } else {
    ALOGD("get source ID failed.");
  }
  return mPhySourceGain;
}

void AudioHwSourceGain::AVSettingObserver::OnSettingUpdate(
    const char* name, const AVSettingValue& value) {
  if (strstr(name, "phySourceGain")) {
    // mAudioHwSourceGain.mPhySourceGain = value.getInt();
    // Read sourceGain by url ;
    mAudioHwSourceGain.getSouceGain();
  }
}

static sp<AudioHwSourceGain> gAudioHwSourceGain = NULL ;
extern "C" int32_t initSourceGain() {
  if (gAudioHwSourceGain == NULL) {
    gAudioHwSourceGain = new AudioHwSourceGain();
  }
  return gAudioHwSourceGain->initSourceGain();
}

extern "C" int32_t getMixGain() {
  if (gAudioHwSourceGain == NULL) {
    gAudioHwSourceGain = new AudioHwSourceGain();
  }
  return gAudioHwSourceGain->mPhySourceGain;
}


