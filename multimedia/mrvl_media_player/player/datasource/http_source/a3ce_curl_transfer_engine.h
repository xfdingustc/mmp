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


#include <stdint.h>

#include <string>
#include <deque>

#include <utils/String8.h>

#include "unicode/regex.h"

#include "http_source/curl_transfer_engine.h"

#ifndef ANDROID_A3CE_CURL_TRANSFER_ENGINE_H
#define ANDROID_A3CE_CURL_TRANSFER_ENGINE_H

namespace android {

class BufferChunk;
class ReadCache;

// A3ceCURLTransferEngine is a specific adaptation of the more general
// CURLTransferEngine.  It provides meaningful implementations of OnHeaderRX,
// RXFunction and OnTransferFinished which allow it to extract the interesting
// HTTP headers and to populate its owner element's data cache.  The underlying
// CURLTransferEngine implementation deals with interfacing with the CURL
// library and with managing the threading involved in runing multiple concurent
// asynchronous HTTP transfers while the A3ceCURLTransferEngine interfaces the
// transfer process with the GStreamer framework.
class A3ceCURLTransferEngine : public CURLTransferEngine {
 public:
  static bool Initialize();
  static void Shutdown();

  A3ceCURLTransferEngine(ReadCache* owner,
                        const std::string& url,
                        size_t new_chunk_size);

  virtual ~A3ceCURLTransferEngine();
  virtual void ResetForNewRequest(const std::string& url);

  // These fields are the cached versions of the interesting headers provided by
  // the server in its initial response to the transfer request.  In particular,
  // the ContentLen is needed by the demux to know how big the "file" is while
  // the ContentType provides the mime-type hint used to know which demux to use
  // to unpack the AV stream.
  uint32_t     MajorVersion() const { return major_version_; }
  uint32_t     MinorVersion() const { return minor_version_; }
  uint32_t     ResultCode()   const { return result_code_; }
  uint64_t     ContentLen()   const { return content_len_; }
  bool         HasContentLen() const { return (found_headers_ & CONTENT_LENGTH_HDR_FLAG); }
  bool         IsChunkedTransfer() const { return (found_headers_ & CHUNKED_ENCODING_FLAG); }
  bool         AcceptBytesRanges() const { return accept_bytes_ranges_; }
  bool         IsPartialContent() const { return is_partial_content_; }
  // ContentType returns just the "type/subtype" portion of the Content-Type
  // header.  Any parameters are ignored.
  const android::String8&  ContentType()  const { return content_type_; }
  // FullContentType returns the entire content of the Content-Type header.
  const android::String8& FullContentType() const { return full_content_type_; }

  // Methods to control the generation of RX reports
  uint32_t GetRXHistoryAmt() const { return rx_history_max_amt_; }
  void     SetRXHistoryAmt(uint32_t val);
  uint32_t GetRXReportPeriod() const { return rx_report_period_; }
  void     SetRXReportPeriod(uint32_t val);
  void     SendRXReport();

  static const uint32_t kDefaultRXHistoryMaxAmt;

 protected:
  // These methods are the overrides of default do-nothing implementations
  // provided by the CURLTransferEngine class.
  virtual size_t OnHeaderRX(void* ptr, size_t size, size_t nmemb);
  virtual size_t RXFunction(void* ptr, size_t size, size_t nmemb);
  virtual void   OnURLChange(const char *url);
  virtual void   OnTransferFinished();
  virtual void   AttentionCallback();
  virtual void   TimerCallback();

 private:
  // Internally, the A3ceCURLTransferEngine needs to implementat a small state
  // machine.  A successful sequence typically involves receiveing some number
  // of temporary failures (redirects) followed by a success, header extraction
  // and finally the content body.  Any permanent failure (400 and 500 status
  // codes) or reaching the end of the header section before finding the
  // required HTTP headers are considered a hard failure.  The State and
  // RequiredHeaderFlags enums are used to track where in the process a transfer
  // engine is at any given point in time.
  enum State {
    WAITING_FOR_HTTP_SUCCESS = 0,
    WAITING_FOR_REQUIRED_HEADERS,
    WAITING_FOR_HEADER_END,
    FINISHED,
  };

  enum RequiredHeaderFlags {
    NO_HEADERS_FOUND        = (0),
    CONTENT_LENGTH_HDR_FLAG = (1 << 0),
    CONTENT_TYPE_HDR_FLAG   = (1 << 1),
    CHUNKED_ENCODING_FLAG   = (1 << 2),
    ALL_HEADERS_FOUND       = (CONTENT_LENGTH_HDR_FLAG |
                               CONTENT_TYPE_HDR_FLAG)
  };

  class RXDataPoint {
   public:
    RXDataPoint(uint64_t rx_time, uint32_t rx_amt) :
      rx_time_(rx_time), rx_amt_(rx_amt) { }

    uint64_t RXTime() const { return  rx_time_; }
    uint32_t RXAmt() const { return  rx_amt_; }

   private:
    uint64_t  rx_time_;
    uint32_t  rx_amt_;
  };

  class RXHistory {
   public:
    RXHistory() : total_amt_(0) { }
    uint64_t GetTotalBytes() const { return total_amt_; }
    uint64_t GetTotalTime() const {
      if (data_.size() <= 1)
        return 0;

      return (data_.back().RXTime() - data_.front().RXTime());
    }

    const RXDataPoint& front() const { return data_.front(); }
    const RXDataPoint& back() const { return data_.back(); }
    size_t size() const { return data_.size(); }

    void clear() {
      total_amt_ = 0;
      data_.clear();
    }

    void push_back(const RXDataPoint& x) {
      total_amt_ += x.RXAmt();
      data_.push_back(x);
    }

    void pop_front() {
      assert(total_amt_ >= front().RXAmt());
      total_amt_ -= front().RXAmt();
      data_.pop_front();
    }

   private:
    uint64_t total_amt_;
    std::deque<RXDataPoint> data_;
  };

  void ResetInternals();
  void PushActiveChunkToOwner();
  void TrimHistory(const uint64_t& now_reference);
  void InternalSendRXReport();

  ReadCache* owner_;
  State state_;
  RequiredHeaderFlags found_headers_;
  uint32_t major_version_;
  uint32_t minor_version_;
  uint32_t result_code_;
  uint64_t content_len_;
  android::String8 content_type_;
  android::String8 full_content_type_;
  android::String8 transfer_encoding_type_;
  pthread_mutex_t header_parse_lock_;

  pthread_mutex_t data_rx_lock_;
  BufferChunk* active_buffer_chunk_;
  uint64_t rx_offset_;

  bool transfer_should_be_paused_;
  bool accept_bytes_ranges_;
  bool is_partial_content_;

  RXHistory rx_history_now_trimmed_;
  RXHistory rx_history_end_trimmed_;
  uint32_t rx_history_max_amt_;
  uint32_t rx_report_period_;

  static RegexMatcher* http_result_parser_;
  static RegexMatcher* content_len_parser_;
  static RegexMatcher* content_type_parser_;
  static RegexMatcher* chunked_transfer_parser_;
  static RegexMatcher* end_of_headers_parser_;
};

}  // namespace android
#endif  // ANDROID_A3CE_CURL_TRANSFER_ENGINE_H
