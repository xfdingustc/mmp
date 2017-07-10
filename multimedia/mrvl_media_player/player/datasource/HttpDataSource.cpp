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

extern "C" {
#include <fcntl.h>
}

#include <utils/Log.h>
#include <utils/CallStack.h>
#include <KeyedVector.h>

#include "http_source/buffers.h"
#include "http_source/curl_transfer_engine.h"
#include "http_source/a3ce_curl_transfer_engine.h"
#include "MediaPlayerOnlineDebug.h"
#include "HttpDataSource.h"

#undef  LOG_TAG
#define LOG_TAG "HttpDataSource"

namespace mmp {

//const char* const HttpDataSource::kDefaultUserAgent = "Android/ICS";
const char* const HttpDataSource::kDefaultUserAgent = "Mozilla/5.0 (iPad; U; "
    "CPU iPhone OS 4_0 like Mac OS X; en-us) AppleWebKit/532.9 "
    "(KHTML, like Gecko) Version/4.0.5 Mobile/8A293 Safari/6531.22.7";
const uint32_t HttpDataSource::kDefaultMaxRAM = (1 << 20);
const uint32_t HttpDataSource::kDefaultCacheBlockSize = (16 << 10);
const uint32_t HttpDataSource::kTailCacheSize = (16 << 10);

Mutex HttpDataSource::init_mutex_;
bool HttpDataSource::init_done_;

HttpDataSource::HttpDataSource(const char *url) {

  ALOGI("HttpDataSource %s", url);

  // Tell ffmpeg whether http server supports seek:
  // is_streamed = 1, no; is_streamed = 0, yes.
  context_->seekable = 0;

  reading_ = false;
  stopping_ = false;
  setup_done_ = false;
  is_stopped_state_ = true;

  prepare_started_ = false;
  prepare_finished_ = false;
  file_len_valid_ = false;
  file_len_ = 0;
  next_read_location_ = 0;
//  prepare_callback_ = NULL;
  tail_cache_ = NULL;
  http_cookies_ = NULL;
  http_user_agent_ = NULL;
  http_headers_ = NULL;
//  buffer_tracker_ = NULL;
  data_cache_ = new ReadCache(this);
  if (NULL != url) {
    source_url_ = strdup(url);
  } else {
    source_url_ = NULL;
  }

  if (!init_done_) {
    init_mutex_.lock();
    if (!init_done_) {
      if (DoStaticInit())
        init_done_ = true;
    }
    init_mutex_.unlock();
  }

  pipe_[0] = -1;
  pipe_[1] = -1;
  pipe(pipe_);
  fcntl(pipe_[0], F_SETFL, O_NONBLOCK);

  if (!DoSetup()) {
    prepared_ = false;
  } else {
    is_stopped_state_ = false;
    prepared_ = true;
  }

  ALOGI("HttpDataSource done");
}

HttpDataSource::~HttpDataSource() {
  ALOGI("%s() line %d, begin to destroy HttpDataSource", __FUNCTION__, __LINE__);
  DoCleanup();

  if (NULL != data_cache_) {
    delete data_cache_;
    data_cache_ = NULL;
  }

  if (NULL != tail_cache_) {
    delete[] tail_cache_;
    tail_cache_ = NULL;
  }

  if (NULL != source_url_) {
    free(source_url_);
    source_url_ = NULL;
  }

  if (NULL != http_cookies_) {
    free(http_cookies_);
    http_cookies_ = NULL;
  }

  if (NULL != http_user_agent_) {
    free(http_user_agent_);
    http_user_agent_ = NULL;
  }

  if (http_headers_) {
    delete http_headers_;
    http_headers_ = NULL;
  }

//  if (NULL != buffer_tracker_)
//    delete buffer_tracker_;

  close(pipe_[0]);
  close(pipe_[1]);

  ALOGI("%s() line %d, HttpDataSource is destroyed", __FUNCTION__, __LINE__);
}

bool HttpDataSource::DoSetup()
{
  if (setup_done_)
    return true;

  if (data_cache_ == NULL || source_url_ == NULL)
    return false;

  if (!data_cache_->Setup(source_url_,
                              kDefaultCacheBlockSize,
                              kDefaultUserAgent,
                              http_cookies_,
                              http_headers_)) {
    ALOGI("DoSetup failed");
    return false;
  }

  data_cache_->SetMaxRAM(kDefaultMaxRAM);
  setup_done_ = true;

  ALOGI("DoSetup done");

  return true;
}

void HttpDataSource::DoCleanup()
{
  if (!setup_done_)
    return;

  if (NULL != data_cache_)
    data_cache_->Reset();

  setup_done_ = false;
}

int64_t HttpDataSource::seekToByte(int64_t offset, int whence) {
  Autolock lock(&mutex_);

  ALOGI("seekToByte %lld, %d", offset, whence);

  int64_t target_offset = 0;

  switch (whence) {
  case SEEK_SET:
    if (offset < 0) {
#if 0
      CallStack stack;
      stack.update();
      stack.dump(LOG_TAG);
#endif
      return -1;
    }
    target_offset = offset;
    break;

  case SEEK_CUR:
    target_offset = next_read_location_ + offset;
    break;

  case SEEK_END:
    target_offset = file_len_ + offset;
    break;

  case AVSEEK_SIZE:
    return file_len_;

  default:
    return -1;
  }

  next_read_location_ = target_offset;
//  is_stopped_state_ = false;

  return target_offset;
}

uint32_t HttpDataSource::read(uint8_t* buffer, int amt) {
  Autolock lock(&mutex_);

//  LOGI("read at %lld, %d bytes", next_read_location_, amt);

  uint64_t pos = next_read_location_;
  uint32_t amt_read = 0;

  if (data_cache_ == NULL) {
    ALOGE("NULL data cache during HTTPSource::read from %lld len %d", pos, amt);
    return -1;
  }

  if (buffer == NULL) {
    ALOGE("NULL target buffer during HTTPSource::read from %lld len %d",
         pos, amt);
    return 0;
  }

  if (is_stopped_state_)
    return -1;

  if (!DoSetup())
    return -1;

  // If we have a tail cache, and this read op is a cache hit, go ahead and
  // satisfy the read right now and don't disturb the underlying HTTP cache
  // layer.
  if (tail_cache_ && (pos >= (file_len_ - kTailCacheSize))) {
    if (pos < file_len_) {
      uint32_t amt_left = static_cast<uint32_t>(file_len_ - pos);
      uint32_t offset;
      amt_read = amt_left < (uint32_t)amt ? amt_left : (uint32_t)amt;
      offset = static_cast<uint32_t>(pos + kTailCacheSize - file_len_);
      memcpy(buffer, tail_cache_ + offset, amt_read);
    }
  } else {

    // adjust amt to read
    if (file_len_valid_) {
      if (pos + amt > file_len_) {
        if (pos >= file_len_) {
          return 0;
        }
        amt = (int)(file_len_ - pos);
      }
    }

    while (amt_read < (uint32_t)amt) {
      ReadCache::ReadDataResult res;
      uint32_t read_this_time;

      // If we have a read timeout callback target, ask it what the current read
      // timeout should be.  Otherwise, no timeout.
      int timeout = -1;
//      if (NULL != rd_timeout_cb_tgt_)
//        timeout = rd_timeout_cb_tgt_->getCurrentReadTimeout();

      reading_ = true;
      mutex_.unlock();

      res = data_cache_->ReadData(reinterpret_cast<uint8_t*>(buffer) + amt_read,
                                  pos + amt_read,
                                  amt - amt_read,
                                  &read_this_time,
                                  timeout,
                                  pipe_[0]);

      mutex_.lock();
      reading_ = false;

      if (stopping_) {
        ALOGI("=== ReadData aborted ===");

        char drain_buffer[16];
        while (::read(pipe_[0], drain_buffer, sizeof(drain_buffer)) > 0)
          continue;

        stopping_ = false;
        //stopped_.signal();

        amt_read = -1;
        break;
      }

      if (read_this_time != static_cast<uint32_t>(-1))
        amt_read += read_this_time;

      // If we have received an explicit OK result code from the low level, then
      // we are finished, no matter how much we actually managed to read.
      if (ReadCache::RDRES_OK == res)
        break;

      if ((ReadCache::RDRES_TIMEOUT == res) &&
          (!data_cache_->GetTransferEngine()->HasContentLen())) {
        // If we don't get the Content-Length from HTTP header,
        // flag the failure by returning a negative result for the read
        // operation and get out.
        ALOGI("timeout and no content length");
        amt_read = -1;
        break;
      }

      // If we timed out, and we have a read timeout callback target, then ask
      // it if we should continue reading or not.
//      if ((ReadCache::RDRES_TIMEOUT == res) &&
//          (NULL != rd_timeout_cb_tgt_) &&
//          rd_timeout_cb_tgt_->onReadTimeout()) {
//        // Looks like we keep reading.
//        continue;
//      }

      // Looks like we enountered some form of an explicit abort signal,
      // unresolvable timeout or fatal error.  Flag the failure by returning a
      // negative result for the read operation and get out.
      amt_read = -1;
      break;
    }
  }

// fix is_streamed regarding "Content-Range"
  if (context_->seekable == 0) {
    A3ceCURLTransferEngine* engine = data_cache_->GetTransferEngine();
    if (engine->IsPartialContent()) {
      // Whether server supports seek: seekable = 0, no; seekable = 1, yes.
      context_->seekable = 1;
      ALOGI("%s() line %d, Change is_streamed to 0", __FUNCTION__, __LINE__);
    }
  }
  if (amt_read != static_cast<uint32_t>(-1))
    next_read_location_ = pos + amt_read;

//  DumpData(buffer, amt_read);
  return amt_read;
}

bool HttpDataSource::stopDataSource() {
  ALOGI("stopDataSource");

#if 0
  CallStack stack;
  stack.update();
  stack.dump(LOG_TAG);
#endif

  Autolock lock(&mutex_);

  is_stopped_state_ = true;

  if (!reading_) {
    return true;
  }

  stopping_ = true;
  char foo = '1';
  write(pipe_[1], &foo, 1);
  //stopped_.wait(mutex_);

  ALOGI("stopDataSource done");

  return true;
}

bool HttpDataSource::startDataSource() {
  ALOGI("startDataSource");
  is_stopped_state_ = false;
  return true;
}

AVInputFormat* HttpDataSource::getAVInputFormat() {
  AVInputFormat *fmt = NULL;

  ALOGI("getAVInputFormat called");

  if (is_stopped_state_) {
    ALOGI("getAVInputFormat called in stopped state");
    return NULL;
  }

  AVProbeData probe_data, *pd = &probe_data;
  static const int probe_data_size = 1024*64;

  memset(pd, 0, sizeof(AVProbeData));

  pd->buf_size = probe_data_size;
  pd->buf = (uint8_t*)malloc(pd->buf_size);

  if (pd->buf) {
    seekToByte(0, SEEK_SET);

    // read 1 byte first, to make it connected
    int amt = read(pd->buf, 1);
    if (amt <= 0) {
      ALOGI("getAVInputFormat failed");
      free(pd->buf);
      pd->buf = NULL;
    } else {
      amt = read(pd->buf + 1, pd->buf_size - 1);
      if (amt <= 0) {
        ALOGI("getAVInputFormat failed");
        free(pd->buf);
        pd->buf = NULL;
      } else {
        ALOGD("total bytes for probe: %d", amt + 1);
        pd->buf_size = amt + 1;
        seekToByte(0, SEEK_SET);
      }
    }
  }

  if (pd->buf) {
    fmt = av_probe_input_format(pd, 1);
    free(pd->buf);
  }

  if (fmt) {
    ALOGI("%s() line %d, AVInputFormat: %s", __FUNCTION__, __LINE__, fmt->name);
  } else {
    ALOGE("%s() line %d, AVInputFormat is not detected", __FUNCTION__, __LINE__);
  }

  return fmt;
}

bool HttpDataSource::isUrlHttp(const char* url)
{
  if (beginsWith(url, "http://") || beginsWith(url, "https://")) {
    if (!beginsWith(url, "http://localhost") && !beginsWith(url, "https://localhost")) {
      return true;
    }
  }
  return false;
}

bool HttpDataSource::beginsWith(const char *const str, const char *const prefix)
{
  if (str == NULL || prefix == NULL)
    return false;

  size_t len1 = strlen(str);
  size_t len2 = strlen(prefix);

  if (len1 < len2)
    return false;

  return strncasecmp(str, prefix, len2) == 0;
}

void HttpDataSource::DumpData(uint8_t *buffer, uint32_t size)
{
#if 0
  char out[128];
  int p = 0;
  for (int i = 0; i < 16; i++) {
    out[p++] = "0123456789ABCDEF"[buffer[i] >> 4];
    out[p++] = "0123456789ABCDEF"[buffer[i] & 0xF];
    out[p++] = ' ';
  }
  out[p] = 0;
  ALOGI("%s", out);

  p = 0;
  for (int i = 0; i < 16; i++) {
    out[p++] = isprint(buffer[i]) ? buffer[i] : '.';
  }
  out[p] = 0;
  ALOGI("%s", out);
#endif
}

bool HttpDataSource::DoStaticInit()
{
  if (!A3ceCURLTransferEngine::Initialize())
    return false;
  if (!CURLTransferEngine::Initialize())
    return false;
  return true;
}

void HttpDataSource::OnConnectionReady(bool success) {
//  assert(!prepare_finished_);
//  assert(NULL != prepare_callback_);

  if (success) {
    // Cache the content length as the file length.  In the future, if the cache
    // is driven to a new location in the file, the content length which comes
    // back might only represent a piece of the length of the total file.  Just
    // after the connection becomes ready, however, we know that the content
    // length represents the total length of the file.
    assert(data_cache_);
    A3ceCURLTransferEngine* engine = data_cache_->GetTransferEngine();
    assert(engine);
    file_len_ = engine->ContentLen();
    file_len_valid_ = true;
    // Whether server supports seek: is_streamed = 1, no; is_streamed = 0, yes.
    context_->seekable = engine->AcceptBytesRanges() ? 1 : 0;
    ALOGI("is_streamed: %d, file_length: %lld", !context_->seekable, file_len_);
    prepare_finished_ = true;
//    prepare_callback_->notifySourcePrepared(kPR_Prepared);
  } else {
//    prepare_callback_->notifySourcePrepared(kPR_PrepareError);
  }
}

void HttpDataSource::OnURLChange(const char *url) {
}

bool HttpDataSource::getParamValue(const char* name, int &value) const {
  return false;
}

} // namespapce android

