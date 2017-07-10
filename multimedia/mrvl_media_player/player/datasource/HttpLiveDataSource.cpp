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
#include <stdio.h>
#include <sys/prctl.h>
}

#include "ChromiumHTTPDataSource.h"
#include "MediaPlayerOnlineDebug.h"
#include "Httplive_Utils.h"
#include "HttpLiveDataSource.h"

#undef  LOG_TAG
#define LOG_TAG "HttpLiveDataSource"

namespace mmp{

extern uint64_t OnlineDebugBitMask;

HttpLiveDataSource::HttpLiveDataSource(
    const char* url, const KeyedVector<String8, String8> *headers)
    : source_url_(NULL),
      headers_(NULL),
      looper_handler_id_(-1),
      source_offset_(0),
      during_download_(false),
      hls_dump_file_(NULL) {
  HLS_LOGI("begin to create HttpLiveDataSource");
  if (NULL != url) {
    source_url_ = strdup(url);
  }
  if (NULL != headers_) {
    setHTTPHeaders(headers);
  }

  HLS_LOGI("HttpLiveDataSource is created");
}

HttpLiveDataSource::~HttpLiveDataSource() {
  HLS_LOGI("begin to destroy HttpLiveDataSource");

  during_download_ = false;

  if (data_source_ != NULL) {
    data_source_ = NULL;
  }

  if (live_session_ != NULL) {
    live_session_->disconnect();
    live_session_->registerCallback(NULL);
    live_session_ = NULL;
  }

  if (looper_ != NULL) {
    looper_->unregisterHandler(looper_handler_id_);
    looper_->stop();
    looper_ = NULL;
  }

  if (NULL != source_url_) {
    free(source_url_);
    source_url_ = NULL;
  }
  if (NULL != headers_) {
    delete headers_;
    headers_ = NULL;
  }

  if (hls_dump_file_) {
    fclose(hls_dump_file_);
    hls_dump_file_ = NULL;
  }

  HLS_LOGI("HttpLiveDataSource is destroyed");
}

bool HttpLiveDataSource::OnHttpLiveEvent(HLSEvent what, int extra) {
  double percent_got = 0.0;
  char *ip_addr = NULL;
  switch (what) {
    case HLS_PLAYING_BANDWIDTH:
      HLS_LOGI("bandwidth change, new bandwidth is %d bps", extra);
      if (cb_target_) {
        cb_target_->OnDataSourceEvent(DS_HLS_PLAYING_BANDWIDTH, extra);
      }
      break;

    case HLS_DOWNLOAD_PROGRESS_IN_PERCENT:
      if (cb_target_) {
        percent_got = (double)extra / 100;
        HLS_LOGV("we have got %.2f data", percent_got);
        cb_target_->OnDataSourceEvent(DS_HLS_DOWNLOAD_PROGRESS_IN_PERCENT, extra);
      }
      during_download_ = true;
      break;

    case HLS_DOWNLOAD_PROGRESS_IN_BYTES:
      if (cb_target_) {
        cb_target_->OnDataSourceEvent(DS_HLS_DOWNLOAD_PROGRESS_IN_BYTES, extra);
      }
      during_download_ = true;
      break;

    case HLS_DOWNLOAD_ERROR:
      HLS_LOGE("received error %d from LiveSession", extra);
      during_download_ = false;
      if (cb_target_) {
        cb_target_->OnDataSourceEvent(DS_HLS_DOWNLOAD_ERROR, extra);
      }
      break;

    case HLS_DOWNLOAD_PLAYLIST_ERROR:
      HLS_LOGE("received error DOWNLOAD_PLAYLIST_ERROR from LiveSession");
      during_download_ = false;
      if (cb_target_) {
        cb_target_->OnDataSourceEvent(DS_HLS_DOWNLOAD_PLAYLIST_ERROR, extra);
      }
      break;

    case HLS_DOWNLOAD_TS_ERROR:
      HLS_LOGE("received error DOWNLOAD_TS_ERROR from LiveSession");
      during_download_ = false;
      if (cb_target_) {
        cb_target_->OnDataSourceEvent(DS_HLS_DOWNLOAD_TS_ERROR, extra);
      }
      break;

    case HLS_REALTIME_SPEED:
      HLS_LOGI("Network download speed is %d", extra);
      if (cb_target_ && during_download_) {
        cb_target_->OnDataSourceEvent(DS_HLS_REALTIME_SPEED, extra);
      }
      break;

    default:
      HLS_LOGE("received unexpected event(what=%d, extra=%d) from LiveSession", what, extra);
      break;
  }

  return true;
}

bool HttpLiveDataSource::setHTTPHeaders(const KeyedVector<String8, String8>* headers) {
  if (headers_ != NULL)
    return false;
  if (headers == NULL)
    return true;
  // Clone the headers so that it can be used by LiveSession
  headers_ = new KeyedVector<android::String8, android::String8>();
  if (headers_ == NULL)
    return false;
  if (headers_->setCapacity(headers->size()) == android::NO_MEMORY) {
    delete(headers_);
    headers_ = NULL;
    return false;
  }
  for (size_t i = 0; i < headers->size(); i++) {
    const String8& key = headers->keyAt(i);
    const String8& value = headers->valueFor(key);
    headers_->add(key, value);
  }
  return true;
}

bool HttpLiveDataSource::startDataSource() {
  if (!prepared_) {
    looper_ = new android::ALooper();
    if (looper_->start() == android::OK) {
      live_session_ = new LiveSession();
      if (live_session_ != NULL) {
        live_session_->registerCallback(this);
        looper_handler_id_ = looper_->registerHandler(live_session_);
        if (looper_handler_id_ != android::INVALID_OPERATION) {
          live_session_->connect(source_url_, headers_);
          // Use a temporary variable to hold onto reference to datasource
          // while we cast and add ref to our member variable.
          android::sp<android::DataSource> live_data_source(live_session_->getDataSource());
          data_source_ = static_cast<android::LiveDataSource*>(live_data_source.get());
          prepared_ = true;
        }
      }
    }
  }

  return prepared_;
}

bool HttpLiveDataSource::stopDataSource() {
  HLS_LOGI("begin to stop data source");
  bool ret = false;
  if (live_session_ != NULL) {
    live_session_->disconnect();
    ret = true;
  }
  during_download_ = false;
  HLS_LOGI("stop data source done");
  return ret;
}

uint32_t HttpLiveDataSource::read(uint8_t* buf, int amt) {
  HLS_LOGV("amt %d, source_offset_ %lld", amt, source_offset_);
  int size_read = 0;
  if (data_source_ != NULL) {
    size_read = data_source_->readAt(source_offset_, buf, amt);
  }
  if (size_read < 0) {
    HLS_LOGI("AVERROR_EOF, err = %d", size_read);
    size_read = AVERROR_EOF;
    during_download_ = false;
    if (cb_target_) {
      cb_target_->OnDataSourceEvent(DS_HLS_DOWNLOAD_EOS, 0);
    }
  } else {
    source_offset_ += size_read;
  }

  if (DATASOURCE_HLS_DUMP_DATA) {
    if (!hls_dump_file_) {
      hls_dump_file_ = fopen("/tmp/media/HLS_dump_data.dat", "wa");
    }
    if (hls_dump_file_ && size_read > 0) {
      fwrite(buf, size_read, 1, hls_dump_file_);
      fflush(hls_dump_file_);
    }
  }

  return size_read;
}

bool HttpLiveDataSource::isSeekable() {
  HLS_LOGI("HttpLiveDataSource can do seek");
  return true;
}

bool HttpLiveDataSource::seekToUs(int64_t seek_pos_us) {
  if (data_source_ != NULL) {
    // Because we have called stopDataSource() which will queue an EOS, so we'd better
    // reset LiveDataSource so that we won't get an EOS after seek.
    data_source_->reset();
  }
  if ((live_session_ != NULL) && (seek_pos_us >= 0) && live_session_->isSeekable()) {
    HLS_LOGI("seek to %lld us", seek_pos_us);
    live_session_->seekTo(seek_pos_us);
    return true;
  }
  return false;
}

int64_t HttpLiveDataSource::seekToByte(int64_t offset, int whence) {
  int64_t target = -1;
  switch(whence) {
    case SEEK_SET:
      target = offset;
      break;
  }

  return target;
}

bool HttpLiveDataSource::getDurationUs(int64_t* duration_us) {
  if (duration_us != NULL) {
    // getDuration returns -1 for live content.
    if ((live_session_ != NULL) && (live_session_->getDuration(duration_us) == android::OK)) {
      // For live content that has indeterminate duration, the value returned is
      // (uint64_t)(-1) which essentially ends up looking like a very very long
      // piece of content.
      if (duration_us < 0) {
        *duration_us = static_cast<uint64_t>(-1);
      }
      HLS_LOGV("stream duration: %lld us", *duration_us);
      return true;
    }
  }
  return false;
}

bool HttpLiveDataSource::getLastSeekMediaTime(int64_t* media_time_ms) const {
  if (media_time_ms != NULL) {
    *media_time_ms = live_session_->getLastSeekMediaTime();
    return true;
  }
  return false;
}


AVInputFormat* HttpLiveDataSource::getAVInputFormat() {
  for (AVInputFormat* fmt = av_iformat_next(NULL); (NULL != fmt); (fmt = av_iformat_next(fmt))) {
    if (NULL == fmt->name) {
      continue;
    }
    if (0 == strcasecmp(fmt->name, "mpegts")) {
      HLS_LOGI("Http Live Streaming will always be MPEG-TS");
      return fmt;
    }
  }
  HLS_LOGE("Didn't get AVInputFormat Http Live Streaming");
  return NULL;
}

/*static function*/
bool HttpLiveDataSource::isUrlHLS(const char* url) {
  HLS_LOGV("Is url HLS: %s", url);
  if (!url) {
    return false;
  }

  // Firstly check if it ends with ".m3u8".
  char *match = strstr(url, ".m3u8");
  if (match) {
    int url_len = strlen(url);
    int ext_len = strlen(".m3u8");
    if (url_len > ext_len) {
      const char *end = url + (url_len - ext_len);
      if (!strcmp(end, ".m3u8")) {
        HLS_LOGI("The url %s is http live streaming, return true", url);
        return true;
      }
    }
  }

  // If it does not end with ".m3u8",
  // try to read some content from the url and search some key words.
  sp<ChromiumHTTPDataSource> temp_HTTPDataSource = new ChromiumHTTPDataSource();
  temp_HTTPDataSource->connect(url);
  char *data = NULL;
  data = (char *)malloc(1024);
  if (data) {
    // Read 1k data to probe if it is Http Live Streaming
    temp_HTTPDataSource->readAt(0, data, 1024);
    HLS_LOGV("what's read: %s", data);
    if ((!strncmp(data, "#EXTM3U", 7)) ||
        strstr(data, "#EXT-X-STREAM-INF:") ||
        strstr(data, "#EXT-X-TARGETDURATION:") ||
        strstr(data, "#EXT-X-MEDIA-SEQUENCE:")) {
      HLS_LOGI("The url %s is http live streaming, return true", url);
      free(data);
      data = NULL;
      return true;
    }
  }

  if (data) {
    free(data);
    data = NULL;
  }
  return false;
}

}

