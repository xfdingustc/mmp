/*
 **                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.
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

#include "MHALSubtitleEngine.h"

#include "ASSParser.h"
#include "TimedText3GPPParser.h"

#undef  LOG_TAG
#define LOG_TAG "MHALSubtitleEngine"

#if PLATFORM_SDK_VERSION >= 18
#include <android/native_window.h>
#endif

#include <gui/ISurfaceComposerClient.h>
#include <gui/SurfaceComposerClient.h>

#include "SkBlurMaskFilter.h"
#include "SkTypeface.h"

#include <SkGlyphCache.h>
#include <SkAutoKern.h>
#include <SkUtils.h>

namespace mmp {

MmpError MHALSubtitleSinkPad::ChainFunction(MmpBuffer* buf) {

  M_ASSERT_FATAL(buf, MMP_PAD_FLOW_ERROR);

  MHALSubtitleEngine* sub_engine = reinterpret_cast<MHALSubtitleEngine*>(GetOwner());
  M_ASSERT_FATAL(sub_engine, MMP_PAD_FLOW_ERROR);

  return sub_engine->feedData(buf);
}


MmpError MHALSubtitleSinkPad::LinkFunction() {
  M_ASSERT_FATAL(peer_, MMP_PAD_FLOW_NOT_LINKED);

  MmpCaps &caps = peer_->GetCaps();

  MHALSubtitleEngine* sub_engine = reinterpret_cast<MHALSubtitleEngine*>(GetOwner());
  M_ASSERT_FATAL(sub_engine, MMP_PAD_FLOW_ERROR);


  const char* mime_type = NULL;
  caps.GetString("mime_type", &mime_type);

  if (mime_type) {
    if (!strcmp(mime_type, "text/x-ssa")) {
      sub_engine->subtitle_parser_ = new ASSParser;
    } else if (!strcmp(mime_type, "text/3gpp-tt")) {
      sub_engine->subtitle_parser_ = new TimedText3GPPParser;
    }
  }
  return MMP_NO_ERROR;

}

MHALSubtitleEngineScheduler::MHALSubtitleEngineScheduler(MHALSubtitleEngine* engine)
  : engine_(engine) {
}

mbool MHALSubtitleEngineScheduler::ThreadLoop() {
  engine_->heartBeat();
  usleep(100 * 1000);
  return true;
}

MHALSubtitleEngine::MHALSubtitleEngine(MRVLSubtitleCbTarget* subCbTgt)
  : clear_screen_time_(0),
  subtitle_parser_(NULL),
  screen_width_(1920),
  screen_height_(1080),
  bottom_margin_(50),
  font_size_(60),
  subCbTgt_(subCbTgt),
  MmpElement("mhal-subtitle-engine") {
  MmpPad* sub_sink_pad = static_cast<MmpPad*>(new MHALSubtitleSinkPad("sink", MmpPad::MMP_PAD_SINK,
                                                  MmpPad::MMP_PAD_MODE_NONE));
  AddPad(sub_sink_pad);

  //scheduler_ = new MHALSubtitleEngineScheduler(this);
  //scheduler_->Run();

  SkTypeface* hira;
  SkScalar dir[3] ={-5,5,5};
  SkMaskFilter *embossMask = SkBlurMaskFilter::CreateEmboss(dir, 0.5,1,2);

  mComposerClient = new SurfaceComposerClient;

  if (NO_ERROR !=  mComposerClient->initCheck()) {
    goto error_out;
  }

  mSurfaceControl = mComposerClient->createSurface(String8("Test Surface"),
      1920, 1080, PIXEL_FORMAT_RGBA_8888, 0);
  if (mSurfaceControl == NULL || !mSurfaceControl->isValid()) {
    goto error_out;
  }

  SurfaceComposerClient::openGlobalTransaction();
  if (NO_ERROR != mSurfaceControl->setLayer(0x7fffffff)){
    goto error_out;
  }
  if (NO_ERROR != mSurfaceControl->hide()) {
    goto error_out;
  }

  if (NO_ERROR != mSurfaceControl->setPosition(0, 0)) {
    goto error_out;
  }
  SurfaceComposerClient::closeGlobalTransaction();

  mSurface = mSurfaceControl->getSurface();
  if (mSurface == NULL) {
    goto error_out;
  }

  window = mSurface.get();

  paint.setARGB(255, 64, 64, 255);
  paint.setTextSize(font_size_);
  paint.setTextAlign(SkPaint::kCenter_Align);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  hira = SkTypeface::CreateFromName("Arial Unicode MS", SkTypeface::kBold);
  SkSafeUnref(paint.setTypeface(hira));

  paint.setMaskFilter(embossMask);
  embossMask->unref();

error_out:
  return;
}

MHALSubtitleEngine::~MHALSubtitleEngine() {
  if (subtitle_parser_) {
    delete subtitle_parser_;
  }
  //scheduler_->RequestExit();
  //scheduler_->Join();

  //delete scheduler_;
  clearScreen();
  flush();
}

MmpError MHALSubtitleEngine::feedData(MmpBuffer *buf) {
  int queued_bytes = 0;

  MmuAutoLock lock(list_lock_);

  if (subtitle_parser_) {
    list<MmpBuffer*> subs = subtitle_parser_->ParsePacket(buf);
    while (!subs.empty()) {
      MmpBuffer* sub_item = (MmpBuffer*)subs.front();
      ready_2_render_list_.push_back(sub_item);
      subs.pop_front();
    }
    subs.clear();
  } else {
    ready_2_render_list_.push_back(buf);
  }

  return MMP_NO_ERROR;
}


void MHALSubtitleEngine::flush() {
  clearScreen();

  MmuAutoLock lock(list_lock_);

  while(!ready_2_render_list_.empty()) {
    MmpBuffer* buf = ready_2_render_list_.front();
    ready_2_render_list_.pop_front();
    delete buf;
  }

  while(!rendering_list_.empty()) {
    MmpBuffer* buf = rendering_list_.front();
    rendering_list_.pop_front();
    delete buf;
  }

}

mbool MHALSubtitleEngine::SendEvent(MmpEvent* event) {
  M_ASSERT_FATAL(event, false);
  switch (event->GetEventType()) {
    case MmpEvent::MMP_EVENT_ACTIVATE:
      SurfaceComposerClient::openGlobalTransaction();
      mSurfaceControl->show();
      SurfaceComposerClient::closeGlobalTransaction();
      break;
    case MmpEvent::MMP_EVENT_DEACTIVATE:
      SurfaceComposerClient::openGlobalTransaction();
      mSurfaceControl->hide();
      SurfaceComposerClient::closeGlobalTransaction();
      break;
    case MmpEvent::MMP_EVENT_UPDATE:
      heartBeat();
      break;
    case MmpEvent::MMP_EVENT_FLUSH_START:
      flush();
      break;
  }
  return true;
}


static inline int is_ws(int c) {
  return !((c - 1) >> 5);
}

static muint32 linebreak(const mchar text[], const mchar stop[],
                         const SkPaint& paint, SkScalar margin) {
  const mchar* start = text;

#if PLATFORM_SDK_VERSION >= 18
  SkAutoGlyphCache    ac(paint, NULL, NULL);
#else
  SkAutoGlyphCache    ac(paint, NULL);
#endif
  SkGlyphCache*       cache = ac.getCache();
  SkFixed             w = 0;
  SkFixed             limit = SkScalarToFixed(margin);
  SkAutoKern          autokern;

  const char* word_start = text;
  int         prevWS = true;

  while (text < stop) {
    const char* prevText = text;
    SkUnichar   uni = SkUTF8_NextUnichar(&text);
    int         currWS = is_ws(uni);
    const SkGlyph&  glyph = cache->getUnicharMetrics(uni);

    if (!currWS && prevWS)
        word_start = prevText;
    prevWS = currWS;

    w += autokern.adjust(glyph) + glyph.fAdvanceX;
    if (w > limit) {
      if (currWS)   {
        // eat the rest of the whitespace
        while (text < stop && is_ws(SkUTF8_ToUnichar(text))) {
          text += SkUTF8_CountUTF8Bytes(text);
        }
      } else {
        // backup until a whitespace (or 1 char)
        if (word_start == start) {
          if (prevText > start) {
            text = prevText;
          }
        } else {
          text = word_start;
        }
      }
      break;
    }
  }
  return text - start;
}

void MHALSubtitleEngine::renderText() {
  MmuAutoLock lock(list_lock_);

#if PLATFORM_SDK_VERSION < 18
  Surface::SurfaceInfo info;
  Region region;;
  mSurface->lock(&info, &region);

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, info.w, info.h, info.s*4);
  bitmap.setPixels(info.bits);

  SkCanvas canvas;
  canvas.setBitmapDevice(bitmap);
#else
  ANativeWindow_Buffer outBuffer;
  mSurface->lock(&outBuffer, NULL);

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, outBuffer.width,
      outBuffer.height, outBuffer.stride * 4);
  bitmap.setPixels(outBuffer.bits);

  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));
#endif

  SkPaint::FontMetrics    metrics;
  int fontHeight = paint.getFontMetrics(&metrics) + 10;
  int marginWidth = screen_width_ - 100;

  muint32 line_num = rendering_list_.size();
  list<MmpBuffer*>::iterator it;

  char* text = NULL;
  char* textStop = NULL;

  for (it = rendering_list_.begin(); it != rendering_list_.end(); ++it) {
    MmpBuffer* buf = *it;
    if (buf) {
      text = (char *)buf->data;
      textStop = text + buf->size;

      while(true) {
        int len = linebreak(text, textStop, paint, marginWidth);
        text += len;
        if (text >= textStop) {
          break;
        }
        line_num++;
      }
    }
  }

  for (it = rendering_list_.begin(); it != rendering_list_.end(); ++it) {
    MmpBuffer* buf = *it;
    if (buf) {
      text = (char *)buf->data;
      textStop = text + buf->size;

      while (true) {
        int len = linebreak(text, textStop, paint, marginWidth);
        muint32 x = screen_width_ / 2;
        muint32 y = screen_height_ - bottom_margin_ - (line_num-- - 1) * fontHeight;
        canvas.drawText(text, len, x, y, paint);
        text += len;
        if (text >= textStop) {
          break;
        }
      }
    }
  }

  mSurface->unlockAndPost();
}

void MHALSubtitleEngine::clearScreen() {
#if PLATFORM_SDK_VERSION < 18
  Surface::SurfaceInfo info;
  Region region;
  mSurface->lock(&info, &region);

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, info.w, info.h, info.s*4);
  bitmap.setPixels(info.bits);

  SkCanvas canvas;
  canvas.setBitmapDevice(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  mSurface->unlockAndPost();
#else
  ANativeWindow_Buffer outBuffer;
  mSurface->lock(&outBuffer, NULL);

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, outBuffer.width,
      outBuffer.height, outBuffer.stride * 4);
  bitmap.setPixels(outBuffer.bits);

  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  mSurface->unlockAndPost();
#endif
}

void MHALSubtitleEngine::heartBeat() {
  MmpClockTime cur_pos;
  MmpClock* clock;

  clock = GetClock();
  if (!clock) {
    return;
  }
  cur_pos = clock_->GetTime();

  mbool dirty = false;
  {
    MmuAutoLock lock(list_lock_);

    if (!ready_2_render_list_.empty()) {
      for (list<MmpBuffer *>::iterator iter = ready_2_render_list_.begin();
          iter != ready_2_render_list_.end();) {
        MmpBuffer* buf = *iter;
        // just delete the outdated subtitle
        if (buf->end_pts < cur_pos) {
          iter = ready_2_render_list_.erase(iter);
          MMP_DEBUG_OBJECT("Delete outdated subtitle");
          delete buf;
        } else if (buf->pts < cur_pos) {
          rendering_list_.push_back(buf);

          ALOGD("text=%s, size=%d, startMs=%d, endMs=%d",
              (char *)(buf->data), buf->size, int(buf->pts/1000), int(buf->end_pts/1000));

          Parcel parcel;
          extractSRTLocalDescriptions(buf->data, buf->size, buf->pts/1000, &parcel);
          subCbTgt_->onSubtitleCome(&parcel);

          iter = ready_2_render_list_.erase(iter);
          MMP_DEBUG_OBJECT("Ready to render one subtitle");
          dirty = true;
        } else {
          iter++;
        }

      }
    }

    if (!rendering_list_.empty()) {
      for (list<MmpBuffer *>::iterator iter = rendering_list_.begin();
          iter != rendering_list_.end();) {
        MmpBuffer* buf = *iter;
        if (buf->end_pts < cur_pos) {
          iter = rendering_list_.erase(iter);
          delete buf;
          MMP_DEBUG_OBJECT("One subtitle finished rendering");
          dirty = true;
        } else {
          iter++;
        }
      }
    }
  }

  if (dirty) {
    renderText();
  }
}

// Parse the SRT text sample, and store the timing and text sample in a Parcel.
// The Parcel will be sent to MediaPlayer.java through event, and will be
// parsed in TimedText.java.
MmpError MHALSubtitleEngine::extractSRTLocalDescriptions(
        const uint8_t *data, ssize_t size, int timeMs, Parcel *parcel) {
    parcel->writeInt32(KEY_LOCAL_SETTING);
    parcel->writeInt32(KEY_START_TIME);
    parcel->writeInt32(timeMs);

    parcel->writeInt32(KEY_STRUCT_TEXT);
    // write the size of the text sample
    parcel->writeInt32(size);
    // write the text sample as a byte array
    parcel->writeInt32(size);
    parcel->write(data, size);

    return MMP_NO_ERROR;
}
}

