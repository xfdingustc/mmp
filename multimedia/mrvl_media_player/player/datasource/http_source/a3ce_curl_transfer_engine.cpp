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

#define LOG_TAG "HttpDataSource"
#include <utils/Log.h>

#include <stdint.h>
#include <stdlib.h>

#include "unicode/decimfmt.h"

#include "http_source/buffers.h"
#include "http_source/a3ce_curl_transfer_engine.h"
#include "http_source/a3ce_curl_transfer_engine_rx_report.h"

namespace android {

const uint32_t A3ceCURLTransferEngine::kDefaultRXHistoryMaxAmt = 2000000;
RegexMatcher* A3ceCURLTransferEngine::http_result_parser_ = NULL;
RegexMatcher* A3ceCURLTransferEngine::content_len_parser_ = NULL;
RegexMatcher* A3ceCURLTransferEngine::chunked_transfer_parser_ = NULL;
RegexMatcher* A3ceCURLTransferEngine::content_type_parser_ = NULL;
RegexMatcher* A3ceCURLTransferEngine::end_of_headers_parser_ = NULL;
// TODO(jsimmons) - use the UNICODE_STRING_SIMPLE macro
static const UnicodeString kHTTPResultRegex(
    "^HTTP/(\\d+)\\.(\\d+)\\s+(\\d\\d\\d)\\s+(.*)(?:\\r\\n)?$");
static const UnicodeString kContentLenRegex(
    "Content-Length:\\s*(\\d+)\\s*$");
static const UnicodeString kContentTypeRegex(
    "Content-Type:\\s*(([^\\s;]+)[ \\t]*(;[^;\\r\\n]+)*)\\s*$");
static const UnicodeString kChunkedTransferRegex(
    "Transfer-Encoding:\\s*(\\w+)\\s*$");
static const UnicodeString kEndOfHeadersRegex("^(?:\\r\\n)?$");

bool A3ceCURLTransferEngine::Initialize() {
  UErrorCode u_error = U_ZERO_ERROR;

  if (!http_result_parser_) {
    http_result_parser_ = new RegexMatcher(
        kHTTPResultRegex, UREGEX_CASE_INSENSITIVE, u_error);
    if (U_FAILURE(u_error)) {
      ALOGE("Unable to create http_result_parser_ err=%d", u_error);
      delete http_result_parser_;
      http_result_parser_ = NULL;
    }
  }

  if (!content_len_parser_) {
    content_len_parser_ = new RegexMatcher(
        kContentLenRegex, UREGEX_CASE_INSENSITIVE, u_error);
    if (U_FAILURE(u_error)) {
      ALOGE("Unable to create content_len_parser_ err=%d", u_error);
      delete content_len_parser_;
      content_len_parser_ = NULL;
    }
  }

  if (!chunked_transfer_parser_) {
    chunked_transfer_parser_ = new RegexMatcher(
        kChunkedTransferRegex, UREGEX_CASE_INSENSITIVE, u_error);
    if (U_FAILURE(u_error)) {
      ALOGE("Unable to create chunked_transfer_parser_ err=%d", u_error);
      delete chunked_transfer_parser_;
      chunked_transfer_parser_ = NULL;
    }
  }

  if (!content_type_parser_) {
    content_type_parser_ = new RegexMatcher(
        kContentTypeRegex, UREGEX_CASE_INSENSITIVE, u_error);
    if (U_FAILURE(u_error)) {
      ALOGE("Unable to create content_len_parser_ err=%d", u_error);
      delete content_type_parser_;
      content_type_parser_ = NULL;
    }
  }

  if (!end_of_headers_parser_) {
    end_of_headers_parser_ = new RegexMatcher(
        kEndOfHeadersRegex, UREGEX_CASE_INSENSITIVE, u_error);
    if (U_FAILURE(u_error)) {
      ALOGE("Unable to create end_of_headers_parser_ err=%d", u_error);
      delete end_of_headers_parser_;
      end_of_headers_parser_ = NULL;
    }
  }

  return (http_result_parser_ && content_len_parser_);
}

void A3ceCURLTransferEngine::Shutdown() {
  if (http_result_parser_) {
    delete http_result_parser_;
    http_result_parser_ = NULL;
  }

  if (content_len_parser_) {
    delete content_len_parser_;
    content_len_parser_ = NULL;
  }

  if (chunked_transfer_parser_) {
    delete chunked_transfer_parser_;
    chunked_transfer_parser_ = NULL;
  }

  if (content_type_parser_) {
    delete content_type_parser_;
    content_type_parser_ = NULL;
  }

  if (end_of_headers_parser_) {
    delete end_of_headers_parser_;
    end_of_headers_parser_ = NULL;
  }
}

A3ceCURLTransferEngine::A3ceCURLTransferEngine(ReadCache* owner,
                                             const std::string& url,
                                             size_t new_chunk_size) :
    CURLTransferEngine(url, new_chunk_size),
    owner_(owner),
    content_type_("unknown"),
    full_content_type_(content_type_),
    active_buffer_chunk_(NULL),
    transfer_should_be_paused_(false),
    accept_bytes_ranges_(false),
    is_partial_content_(false),
    rx_history_max_amt_(kDefaultRXHistoryMaxAmt),
    rx_report_period_(0) {
  assert(owner);
  pthread_mutex_init(&header_parse_lock_, NULL);
  pthread_mutex_init(&data_rx_lock_, NULL);
  ResetInternals();
}

A3ceCURLTransferEngine::~A3ceCURLTransferEngine() {
  ResetInternals();
  pthread_mutex_destroy(&header_parse_lock_);
  pthread_mutex_destroy(&data_rx_lock_);
}

void A3ceCURLTransferEngine::ResetForNewRequest(const std::string& url) {
  CURLTransferEngine::ResetForNewRequest(url);
  ResetInternals();
}

void A3ceCURLTransferEngine::ResetInternals() {
  pthread_mutex_lock(&header_parse_lock_);
  state_ = WAITING_FOR_HTTP_SUCCESS;
  found_headers_ = NO_HEADERS_FOUND;
  major_version_ = 0;
  minor_version_ = 0;
  result_code_ = 0;
  content_len_ = 0;
  transfer_should_be_paused_ = false;
  pthread_mutex_unlock(&header_parse_lock_);

  pthread_mutex_lock(&data_rx_lock_);
  rx_offset_ = 0;
  if (active_buffer_chunk_) {
    delete active_buffer_chunk_;
    active_buffer_chunk_ = NULL;
  }
  rx_history_now_trimmed_.clear();
  rx_history_end_trimmed_.clear();
  pthread_mutex_unlock(&data_rx_lock_);
}

size_t A3ceCURLTransferEngine::OnHeaderRX(void* ptr, size_t size, size_t nmemb) {
  if (!ptr) return -1;

  //LOGI("%s() line %d, HTTP HEADER: %s", __FUNCTION__, __LINE__, (const char *)ptr);

  size_t len = size*nmemb;
  bool found_match = false;
  bool signal_failure = false;
  UErrorCode u_error = U_ZERO_ERROR;

  pthread_mutex_lock(&header_parse_lock_);

  // If server's response contains "Accept-Ranges: bytes" or "Content-Range",
  // then it is seekable during playback.
  static const char *accept_ranges = "Accept-Ranges: bytes";
  if (strncasecmp((const char*)ptr, accept_ranges, 20) == 0) {
    accept_bytes_ranges_ = true;
  }

  static const char *content_range = "Content-Range";
  if (strncasecmp((const char*)ptr, content_range, 13) == 0) {
    is_partial_content_ = true;
  }

  switch (state_) {
    case WAITING_FOR_HTTP_SUCCESS:
    {
      UnicodeString ptr_str(static_cast<const char*>(ptr), len);
      http_result_parser_->reset(ptr_str);

      if (http_result_parser_->matches(u_error)) {
        // Extract the HTTP version info.  We demand that the connection use
        // HTTP 1.1 or better.  Anything less and we should fail this
        // connection.

        UnicodeString major_version_str =
            http_result_parser_->group(1, u_error);
        assert(U_SUCCESS(u_error));

        UnicodeString minor_version_str =
            http_result_parser_->group(2, u_error);
        assert(U_SUCCESS(u_error));

        UnicodeString result_code_str =
            http_result_parser_->group(3, u_error);
        assert(U_SUCCESS(u_error));

        DecimalFormat fmt("#", u_error);
        assert(U_SUCCESS(u_error));
        Formattable result;

        fmt.parse(major_version_str, result, u_error);
        assert(U_SUCCESS(u_error));
        major_version_ = result.getLong();

        fmt.parse(minor_version_str, result, u_error);
        assert(U_SUCCESS(u_error));
        minor_version_ = result.getLong();

        fmt.parse(result_code_str, result, u_error);
        assert(U_SUCCESS(u_error));
        result_code_ = result.getLong();

        found_match = true;

        if (major_version_ >= 1) {
          if ((result_code_ >= 200) && (result_code_ < 300)) {
            // Success!  Move on to looking for the required headers.
            ALOGI("Found HTTP success.  Connection is HTTP/%d.%d, "
                 "code was %d", major_version_, minor_version_, result_code_);
            state_ = WAITING_FOR_REQUIRED_HEADERS;
          } else
          if ((result_code_ < 300) || (result_code_ >= 400)) {
            ALOGE("Bad HTTP/%d.%d result code, code was %d",
                 major_version_, minor_version_, result_code_);
            signal_failure = true;
            state_ = WAITING_FOR_HEADER_END;
          }

        } else {
          ALOGE("Bad HTTP version number (HTTP/%d.%d), code was %d",
               major_version_, minor_version_, result_code_);
          signal_failure = true;
          state_ = WAITING_FOR_HEADER_END;
        }
      }
      break;
    }

    case WAITING_FOR_REQUIRED_HEADERS:
    {
      // According to RFC2616 section 4, if a message is received with
      // both a Transfer-Encoding header field and a Content-Length header
      // field, the latter MUST be ignored.
      // It will check only when both of headers cannot be found.
      if (!found_match && !(found_headers_ & CONTENT_LENGTH_HDR_FLAG) &&
                          !(found_headers_ & CHUNKED_ENCODING_FLAG)) {
        UnicodeString ptr_str(static_cast<const char*>(ptr), len);
        content_len_parser_->reset(ptr_str);
        if (content_len_parser_->matches(u_error)) {
          UnicodeString content_len_str =
              content_len_parser_->group(1, u_error);
          assert(U_SUCCESS(u_error));

          DecimalFormat fmt("#", u_error);
          assert(U_SUCCESS(u_error));
          Formattable result;
          fmt.parse(content_len_str, result, u_error);
          assert(U_SUCCESS(u_error));
          content_len_ = result.getInt64();

          found_match = true;
          found_headers_ = static_cast<RequiredHeaderFlags>
                            (found_headers_ | CONTENT_LENGTH_HDR_FLAG);
          ALOGI("Found content length %lld", content_len_);
        }
      }

      if (!found_match && !(found_headers_ & CONTENT_TYPE_HDR_FLAG)) {
        UnicodeString ptr_str(static_cast<const char*>(ptr), len);
        content_type_parser_->reset(ptr_str);
        if (content_type_parser_->matches(u_error)) {
          UnicodeString content_type_str =
              content_type_parser_->group(1, u_error);
          assert(U_SUCCESS(u_error));
          full_content_type_.setTo(content_type_str.getBuffer(),
                              content_type_str.length());
          full_content_type_.toLower();
          content_type_str =
              content_type_parser_->group(2, u_error);
          assert(U_SUCCESS(u_error));
          content_type_.setTo(content_type_str.getBuffer(),
                              content_type_str.length());
          content_type_.toLower();
          found_match = true;
          found_headers_ = static_cast<RequiredHeaderFlags>
                            (found_headers_ | CONTENT_TYPE_HDR_FLAG);
          ALOGI("Found content type %s", full_content_type_.string());
        }
      }

      if (!found_match && strncasecmp((const char*)ptr, accept_ranges, 20) == 0) {
        accept_bytes_ranges_ = true;
      }

      if (!found_match && strncasecmp((const char*)ptr, content_range, 13) == 0) {
        is_partial_content_ = true;
      }

      if (ALL_HEADERS_FOUND == found_headers_)
        state_ = WAITING_FOR_HEADER_END;

      // If we found a match in this case, then break out.  Otherwise, drop
      // through and check for the end of headers.
      if (found_match)
        break;
    }

    case WAITING_FOR_HEADER_END:
    {
      // For Transfer-Encoding
      if (!found_match && !(found_headers_ & CHUNKED_ENCODING_FLAG)) {
        UnicodeString ptr_str(static_cast<const char*>(ptr), len);
        chunked_transfer_parser_->reset(ptr_str);
        if (chunked_transfer_parser_->matches(u_error)) {
          UnicodeString transfer_encoding_str =
              chunked_transfer_parser_->group(1, u_error);
          assert(U_SUCCESS(u_error));
          transfer_encoding_type_.setTo(transfer_encoding_str.getBuffer(),
                              transfer_encoding_str.length());
          transfer_encoding_type_.toLower();
          found_match = true;
          String8 chunked_string("chunked");
          if (transfer_encoding_type_ == chunked_string) {
            ALOGI("Found Transfer-Encoding %s", transfer_encoding_type_.string());
            found_headers_ = static_cast<RequiredHeaderFlags>
                              (found_headers_ | CHUNKED_ENCODING_FLAG);
            // It needs to ignore content-length header when there is transfer-encodeing header.
            found_headers_ = static_cast<RequiredHeaderFlags>
                              (found_headers_ & ~CONTENT_LENGTH_HDR_FLAG);
            content_len_ = 0;
          }
        }
      }

      UnicodeString ptr_str(static_cast<const char*>(ptr), len);
      end_of_headers_parser_->reset(ptr_str);
      if (end_of_headers_parser_->matches(u_error))
        state_ = FINISHED;
      break;
    }

    case FINISHED:
    default:
      break;
  }

  // If we are done with headers, and this is not a re-direct, and we are
  // missing the content length header, then we have a problem and need to tell
  // the our owner that this connection will fail.
  if ((state_ == FINISHED) &&
     ((result_code_ < 300) || (result_code_ >= 400))) {
    if(!(found_headers_ & CONTENT_LENGTH_HDR_FLAG)) {
      if (found_headers_ & CHUNKED_ENCODING_FLAG) {
        ALOGI("Found all headers, transfer encoding is chunked. Ready to proceed");
      } else {
        ALOGW("Reached end of headers before finding required fields.");
      }
    } else {
      ALOGI("Found all headers, ready to proceed.");
    }
  }

  pthread_mutex_unlock(&header_parse_lock_);

  assert(owner_);
  if (signal_failure)
    owner_->OnConnectionReady(false);
  else if (state_ == FINISHED)
    owner_->OnConnectionReady(true);

  return len;
}

size_t A3ceCURLTransferEngine::RXFunction(void* ptr, size_t size, size_t nmemb) {
  size_t rx_len  = size * nmemb;
  if (!rx_len) return 0;
  if (!ptr)    return 0;

  if (transfer_should_be_paused_)
    return CURL_WRITEFUNC_PAUSE;

  uint8_t* data = static_cast<uint8_t*>(ptr);
  size_t ret_val = 0;
  uint64_t rx_time = GetTickCount();
  pthread_mutex_lock(&data_rx_lock_);

  while (rx_len) {
    // Make a new chunk if we need to.
    if (!active_buffer_chunk_) {
      active_buffer_chunk_ = new BufferChunk(rx_offset_, NewChunkSize());
      if (!active_buffer_chunk_) {
        goto bailout;
      }

      if (!active_buffer_chunk_->Allocate()) {
        delete active_buffer_chunk_;
        active_buffer_chunk_ = NULL;
        goto bailout;
      }
    }

    // Fill as much of the chunk as we can.
    assert(active_buffer_chunk_);
    size_t chunk_space = active_buffer_chunk_->Space();
    size_t to_do = (rx_len > chunk_space) ? chunk_space : rx_len;
    active_buffer_chunk_->AddToChunk(data, to_do);
    data    += to_do;
    rx_len  -= to_do;
    ret_val += to_do;

    // If the chunk is now full, hand it off to our owner.
    if (!active_buffer_chunk_->Space())
      PushActiveChunkToOwner();
  }

  // Record our RX history
  { // explicit scoping to avoid goto/initializer warning.
    RXDataPoint data_point(rx_time, size*nmemb);
    rx_history_now_trimmed_.push_back(data_point);
    rx_history_end_trimmed_.push_back(data_point);
    TrimHistory(rx_time);
  }

bailout:
  pthread_mutex_unlock(&data_rx_lock_);
  return ret_val;
}

void A3ceCURLTransferEngine::OnURLChange(const char *url) {
  assert(owner_);

  owner_->OnURLChange(url);
}

void A3ceCURLTransferEngine::OnTransferFinished() {
  pthread_mutex_lock(&data_rx_lock_);

  if (TransferResult() == CURLE_OK) {
    if (active_buffer_chunk_)
      PushActiveChunkToOwner();
  } else {
    // Any chunk is progress should be discarded at this point.
    delete active_buffer_chunk_;
    active_buffer_chunk_ = NULL;

    // If the transfer finished with a low level networking error, wakeup any
    // pending reader so they can figure out the place in the buffer cache where
    // we should attempt to reconnect and start filling in the cache again.  If
    // we have not finished parsing our headers yet, be sure to also indicate a
    // failure to become prepared.
    assert(owner_);
    if (state_ != FINISHED) {
      ALOGE("CURL Transfer finished with error (%d) before all headers have been"
           " received.  Aborting connection ready.", TransferResult());
      owner_->OnConnectionReady(false);
    }

    ALOGW("CURL Transfer finished with error (%d), waking buffer level in an"
         " attempt to recover.", TransferResult());
    owner_->WakeupReader();

  }

  pthread_mutex_unlock(&data_rx_lock_);
}

void A3ceCURLTransferEngine::PushActiveChunkToOwner() {
  // TODO(johngro) : assert that we are holding the data_rx_lock.
  BufferChunk* tmp = active_buffer_chunk_;
  rx_offset_ += active_buffer_chunk_->ChunkFilled();
  active_buffer_chunk_ = NULL;

  assert(owner_);
  pthread_mutex_unlock(&data_rx_lock_);

  ReadCache::AddChunkResult acr = owner_->AddChunk(tmp);

  // If we need to seek, put ourselves on the attn. list so we can tear down and
  // rebuild the connection once we get back out to the main work thread loop.
  // Currently, we are in one of CURL's write callbacks and it is not safe to
  // tear down the connection.
  if (acr == ReadCache::ACRES_SEEK_CONNECTION) {
    transfer_should_be_paused_ = false;
    InternalWakeupTransfer();
  } else {
    transfer_should_be_paused_ = (acr != ReadCache::ACRES_OK);
  }

  pthread_mutex_lock(&data_rx_lock_);
}

void A3ceCURLTransferEngine::AttentionCallback() {
  assert(owner_);
  // First thing to do, check to see if we need to perform a seek operation.
  // Calling our owner's LatchTransferRange will check to see if we need to, and
  // will clear the seek flag atomically if it was set.
  uint64_t seek_start, seek_end;
  if (owner_->LatchTransferRange(&seek_start, &seek_end)) {
    transfer_should_be_paused_ = false;

    // If we are seeking, be sure to use the current effective URL for the
    // connection.  If we have been through redirects, and we start at the
    // original base URL, it is possible to actually end up getting redirected
    // to a different piece of content which will totally break the upper level
    // AV demux code.  This behavior was observer with a piece of CNet content
    // being served from a CDN where the original URL actually leads to
    // different copies of the main asset, but with different ads spliced into
    // the front of them.  See Issue 2599534 for details.
    char* effectiveURL = NULL;
    CURLcode cc;
    cc = curl_easy_getinfo(EasyHandle(), CURLINFO_EFFECTIVE_URL, &effectiveURL);
    if ((CURLE_OK == cc) && (NULL != effectiveURL)) {
      ResetForNewRequest(std::string(effectiveURL));
    } else {
      ALOGW("Failed to fetch effective URL from CURL; will fall back on original"
           " base URL \"%s\"", owner_->BaseURL().c_str());
      ResetForNewRequest(owner_->BaseURL());
    }

    SetRXRange(seek_start, seek_end);
    rx_offset_ = GetRXStart();
    BeginTransfer();
  } else {
    // Looks like we don't need to seek.  Now check to see if we are in the
    // wrong paused state.  If we are, go ahead and toggle state telling the
    // CURL layer to pause or resume the transfer as appropriate.
    bool should_be_paused = owner_->GetConnectionShouldPause();
    CURL* curl_easy_handle = EasyHandle();

    if (transfer_should_be_paused_ != should_be_paused) {
      transfer_should_be_paused_ = should_be_paused;

      // nothing to actually do if we have no on-going transfer
      if (curl_easy_handle) {
        if (transfer_should_be_paused_)
          curl_easy_pause(curl_easy_handle, CURLPAUSE_ALL);
        else
          curl_easy_pause(curl_easy_handle, CURLPAUSE_CONT);
      }
    }
  }
}

void A3ceCURLTransferEngine::TrimHistory(const uint64_t& now_reference) {
  // TODO(johngro) : assert that we are holding the rx_data lock.
  while (rx_history_now_trimmed_.size()) {
    int64_t delta = now_reference - rx_history_now_trimmed_.front().RXTime();
    if (delta <= static_cast<int64_t>(rx_history_max_amt_))
      break;
    rx_history_now_trimmed_.pop_front();
  }

  if (rx_history_end_trimmed_.size() > 1) {
    uint64_t tmp = rx_history_end_trimmed_.back().RXTime();

    while (rx_history_end_trimmed_.size()) {
      int64_t delta = tmp - rx_history_end_trimmed_.front().RXTime();
      if (delta <= static_cast<int64_t>(rx_history_max_amt_))
        break;
      rx_history_end_trimmed_.pop_front();
    }
  }
}

void A3ceCURLTransferEngine::TimerCallback() {
  // Produce a periodic RX report if enabled and schedule the next report.
  pthread_mutex_lock(&data_rx_lock_);

  if (rx_report_period_) {
    InternalSendRXReport();

    uint64_t next_report = GetTickCount() + rx_report_period_;
    ScheduleTimerCallback(next_report, false);
  }

  pthread_mutex_unlock(&data_rx_lock_);
}

void A3ceCURLTransferEngine::SetRXHistoryAmt(uint32_t val) {
  pthread_mutex_lock(&data_rx_lock_);
  rx_history_max_amt_ = val;
  pthread_mutex_unlock(&data_rx_lock_);
}

void A3ceCURLTransferEngine::SetRXReportPeriod(uint32_t val) {
  pthread_mutex_lock(&data_rx_lock_);

  uint64_t next_report = kInvalidTickCountTime;
    if (0 != (rx_report_period_ = val))
      next_report = GetTickCount() + rx_report_period_;

  pthread_mutex_unlock(&data_rx_lock_);

  // Scheduling the timer callback grabs the CURLTransferEngine work thread
  // lock.  In order to avoid a possible deadlock situation, we need to make
  // certain that we are not holding the data_rx_lock when this happens.
  ScheduleTimerCallback(next_report, true);
}

void A3ceCURLTransferEngine::SendRXReport() {
  pthread_mutex_lock(&data_rx_lock_);
  InternalSendRXReport();
  pthread_mutex_unlock(&data_rx_lock_);
}

// InternalSendRXReport is called either internally (by the periodic timer
// callback from the lower level CURLHttpTransferEngine work thread) or from the
// pubic method SendRXReport.  In either case, its job is to produce a report
// containing recent bitrate averages (calculated using two different methods)
// up to its owner ReadCache where the data can be merged with the lookahead
// buffer status and posted to the message bus for the gstreamer pipeline which
// this element belongs to.  RXReports are used by the system level to assist in
// determining when an AV pipeline has reached a safe buffering state for
// progressive download AV systems.
void A3ceCURLTransferEngine::InternalSendRXReport() {
  // TODO(johngro) : assert that we are holding the data_rx_lock.

  // Start by observing the internal tick count clock and trimming the received
  // data history so that it does not include extra data.
  uint64_t now = GetTickCount();
  TrimHistory(now);

  if (owner_) {
    // The realtime bitrate of the stream is defined as the amt of data in the
    // history divided by the amt of time betweeen the first data point in the
    // history and what time it is now.  Trimming the realtime history throws
    // away any data points which were received longer ago than the configured
    // threshold.  IOW - it reports the bitrate of the received data stream
    // relative to now.  If the engine pauses because the readahead buffer is
    // full, the observed realtime bitrate will continue to go down over time
    // while the connection is paused.  When there are no data points in the
    // history, the bitrate is simply reported as 0.
    uint64_t realtime_time = rx_history_now_trimmed_.size() ?
                             (now - rx_history_now_trimmed_.front().RXTime()) :
                             0;
    uint64_t realtime_byterate =
      (0 != realtime_time) ?
      ((rx_history_now_trimmed_.GetTotalBytes() * 1000000) / realtime_time) :
      0;

    // The windowed bitrate of the stream is defined as the amt of data in the
    // history divided by the delta in time between the first and the last
    // samples in the history.  Trimming the windowed history throws away any
    // data points which were received longer ago than the configured threashold
    // relative to the most recent sample received.  IOW - it reports the
    // bitrate of the stream the last time it was actively receiving samples.
    // If the engine pauses because the readahead buffer is full, the observed
    // windowed bitrate will continue to report the bitrate the system was
    // receiving just before the pause took place.  The windowed bitrate only
    // has meaning if there are at least 2 samples in the history.  If there are
    // not at least two samples in the buffer, the bitrate will be reported as
    // 0.  The size of the window (delta between first and last sample) is
    // reported as well to allow application to tell the different between
    // actually 0 bitrate and not enough data to know.
    uint64_t windowed_time = rx_history_end_trimmed_.GetTotalTime();
    uint64_t windowed_byterate =
      (0 != windowed_time) ?
      ((rx_history_end_trimmed_.GetTotalBytes() * 1000000) / windowed_time) :
      0;

    // Clamp the reported byterates, just in case they are ever greater than
    // 4GB/sec for whatever reason :)
    if (realtime_byterate > 0xFFFFFFFF)
      realtime_byterate = 0xFFFFFFFF;

    if (windowed_byterate > 0xFFFFFFFF)
      windowed_byterate = 0xFFFFFFFF;

    // Finally, send our report up to our owning data cache.
    owner_->SendRXReport(A3ceCURLTransferEngineRXReport(realtime_byterate,
                                                       windowed_byterate,
                                                       windowed_time));
  }
}

}  // namespace android