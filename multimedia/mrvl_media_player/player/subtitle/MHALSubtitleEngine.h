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

#ifndef MHALSUBTITLE_ENGINE_H
#define MHALSUBTITLE_ENGINE_H

#include "frameworks/mmp_frameworks.h"
#include "MediaPlayerOnlineDebug.h"

#include <stdlib.h>
#include <stdio.h>

#if PLATFORM_SDK_VERSION >= 18
#include <gui/Surface.h>
#include <gui/SurfaceControl.h>
#else
#include <gui/SurfaceComposerClient.h>
#endif
#include <private/gui/ComposerService.h>
#include <ui/GraphicBufferMapper.h>
#include <SkCanvas.h>
#include <SkPaint.h>

#include <list>

#include "SubtitleParser.h"

using namespace std;
namespace mmp {

class MRVLSubtitleCbTarget {
  public:
    virtual ~MRVLSubtitleCbTarget() {};
    virtual MmpError onSubtitleCome(void *data = NULL)=0;
};

class MHALSubtitleSinkPad : public MmpPad {
  public:
    MHALSubtitleSinkPad(mchar* name, MmpPadDirection dir, MmpPadMode mode)
      : MmpPad(name, dir, mode) {
    }
    ~MHALSubtitleSinkPad() {};
    virtual MmpError ChainFunction(MmpBuffer* buf);
    virtual MmpError LinkFunction();
};

class MHALSubtitleEngine;

class MHALSubtitleEngineScheduler : public MmuTask {
  public:
    explicit MHALSubtitleEngineScheduler(MHALSubtitleEngine* engine);
    ~MHALSubtitleEngineScheduler() {};
    virtual mbool ThreadLoop();
  private:
    MHALSubtitleEngine*   engine_;
};

class MHALSubtitleEngine : public MmpElement {
  friend class MHALSubtitleSinkPad;
  friend class MHALSubtitleEngineScheduler;

  public:
    explicit MHALSubtitleEngine(MRVLSubtitleCbTarget* subCbTgt);
    virtual ~MHALSubtitleEngine();
    virtual mbool SendEvent(MmpEvent* event);
    MmpError extractSRTLocalDescriptions(
        const uint8_t *data, ssize_t size, int timeMs, Parcel *parcel) ;
  private:
    void flush() ;
    MmpError feedData(MmpBuffer *buf) ;
    void heartBeat();
  private:
      enum {
      // These keys must be in sync with the keys in TimedText.java
      KEY_START_TIME = 7, // int
      KEY_STRUCT_TEXT = 16, // Text
      KEY_LOCAL_SETTING = 102,
    };

    MRVLSubtitleCbTarget* subCbTgt_;
    list<MmpBuffer*>      ready_2_render_list_;
    list<MmpBuffer*>      rendering_list_;
    MmuMutex              list_lock_;

    //MHALSubtitleEngineScheduler* scheduler_;

    sp<Surface>           mSurface;
    sp<SurfaceComposerClient> mComposerClient;
    sp<SurfaceControl>    mSurfaceControl;
    ANativeWindow*        window;
    SkBitmap              bitmap;
    SkPaint               paint;
    Rect                  tempRect;
    void*                 addr;

    TimeStamp             clear_screen_time_;
    AVCodecContext*       codec_ctx_;
    AVCodec*              codec_;

    muint32               screen_width_;
    muint32               screen_height_;
    muint32               bottom_margin_;
    muint32               font_size_;

    SubtitleParser*       subtitle_parser_;

    void renderText();
    void clearScreen();
};

}
#endif

