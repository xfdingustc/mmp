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


#ifndef ANDROID_BUFFERS_H
#define ANDROID_BUFFERS_H

#include <assert.h>
#include <map>
#include <stdint.h>
#include <string>

#include "http_source/IReadCacheOwner.h"
#include "thread_utils.h"

namespace android {
class String8;
template <typename KEY, typename VALUE> class KeyedVector;
}

namespace android {

using android::String8;
using android::KeyedVector;

class A3ceCURLTransferEngine;
class A3ceCURLTransferEngineRXReport;

// The curlhttpsrc plugin needs to maintain a cache of the data it has already
// received from the network.  The size of the heap allocations for the in-RAM
// portion of the cache are controlled by the application via the blocksize
// paramater common to most gstreamer source elements.  The BufferChunk class is
// the object which represents these cache allocations.  It needs to be able to
// be partially filled, to track its position in the underlying file
// structure, and to have additional data added to it as it is received by the
// network.
class BufferChunk {
 public:
   BufferChunk(uint64_t file_offset, uint32_t chunk_size);
  ~BufferChunk();

  bool Allocate();
  bool AddToChunk(const uint8_t* data, uint32_t data_len);

  uint64_t FileOffset()  const { return file_offset_; }
  uint32_t ChunkSize()   const { return chunk_size_; }
  uint32_t ChunkFilled() const { return chunk_filled_; }
  uint8_t* ChunkData()   const { return chunk_data_; }
  uint32_t Space()       const {
    assert(chunk_filled_ <= chunk_size_);
    return chunk_size_ - chunk_filled_;
  }

 private:
  uint64_t file_offset_;
  uint32_t chunk_size_;
  uint32_t chunk_filled_;
  uint8_t* chunk_data_;
};

// A ReadCache currently serves as the file cache for the curlhttpsrc gstreamer
// source element.  It is made up of BufferChunks and provides an place for a
// reader to read data from the cache in a thread-safe fashion as well as to
// block while waiting for required data to be received while still being able
// to wake up in an event driven fashion in case shutdown is required. It also
// provides a place for the network RX code to push filled buffers and to wakeup
// and threads who might be interested in the results of the received data.
// Internally, the ReadCache uses an STL map indexed by starting file position
// for the chunks to allow ln(N) access for locating data to read as well as for
// pushing arbitrary new chunks of data into the cache.
class ReadCache {
 public:
  friend class A3ceCURLTransferEngine;
  enum AddChunkResult {
    ACRES_FATAL_ERROR = 0,
    ACRES_OK,
    ACRES_PAUSE_CONNECTION,
    ACRES_SEEK_CONNECTION
  };

  enum ReadDataResult {
    RDRES_OK,
    RDRES_TIMEOUT,
    RDRES_EVENT_SIGNALLED,
    RDRES_FATAL_ERROR
  };

  static const uint32_t kMaxConsecutiveBadTransfers;
  static const uint32_t kMinMaxRAM;
  static const uint32_t kMaxMaxRAM;
  static const uint32_t kDefaultMaxRAM;
  static const float kMinReadaheadPercentage;
  static const float kMaxReadaheadPercentage;
  static const float kDefaultReadaheadPercentage;
  static const long kLSThreshBytesPerSec;
  static const long kLSThreshDurationSecs;

  ReadCache(IReadCacheOwner* owner);
  ~ReadCache();

  // Reset will be called in response to gstreamer framework events
  // which require all readers to immediately unblock any blocking read
  // operations and get out (usually because the graph is transitioning to the
  // NULL state during player shutdown).  It will kill all ongoing transfers,
  // dump all data, and reset the cache back to its initial state.
  void Reset();

  // ClearCallbacks will be called while deleting AVAPI media player instance,
  // to prevent memory leaks.
  void ClearCallbacks();

  // Setup is used to start to build a cache.  The url of the content and
  // transfer blocksize must be specified.  In addition, the user agent used for
  // the connection may be overridden by specifying a non-null user-agent
  // parameter.  After being successfully set up, and read cache may not be set
  // up again until it has been reset.
  bool Setup(const char* base_url,
             uint32_t blocksize,
             const char* user_agent,
             const char* cookies,
             const KeyedVector<String8, String8>* headers);

  // Potentially blocking call which will read data from the file cache, waiting
  // for the data to be received if need be.  If the call fails due to a timeout
  // or signalling of the quit event, it may still have read some of the data.
  // The specific amt read (success or failure) will be stored in the amt_read
  // parameter.  Timeout values are specified in milliseconds.  Negative timeout
  // values imply an infinite timeout (consistent with the definition of poll(2))
  ReadDataResult ReadData(uint8_t* buf,
                          uint64_t start,
                          uint32_t len,
                          size_t* amt_read,
                          int timeout_msec = -1,
                          int quit_event = -1);

  uint64_t FileLen() const { return file_len_; }
  bool     SetMaxRAM(uint32_t max_ram);
  uint32_t GetMaxRAM() const { return max_ram_; }

  bool  SetReadaheadPercentage(float readahead_percentage);
  float GetReadaheadPercentage() const { return readahead_percentage_; }

  bool GetConnectionShouldPause() const { return connection_should_pause_; }
  A3ceCURLTransferEngine* GetTransferEngine() const { return transfer_; }

  // Report the number of bytes cached and available ahead of the read pointer.
  uint64_t GetBufferedBytes();

 private:
  // check to see if a given timeout value is considered to be infinite as
  // defined by the poll(2) manpage.
  static inline bool IsInfiniteTimeout(int timeout) {
    return (timeout < 0);
  }

  // AddChunk is called by the underlying transfer engine there is a filled
  // chunk ready to be added to the cache.
  AddChunkResult AddChunk(BufferChunk* chunk);

  // WakeupReader is called in order to wake up any reader who might be blocked
  // waiting for interesting data to come back.
  void WakeupReader() { interesting_data_evt_.setEvent(); }

  // SendRXReport is called by the underlying transfer engine when there is an
  // RX report to create and send to the gstreamer level.
  void SendRXReport(const A3ceCURLTransferEngineRXReport& report);

  uint32_t MaxReadaheadBytes() const {
    return static_cast<uint32_t>(max_ram_ * readahead_percentage_);
  }
  const std::string& BaseURL() const { return base_url_; }

  uint64_t RoundDownToBlockOffset(uint64_t offset) const {
    return offset - (offset % blocksize_);
  }

  void OnConnectionReady(bool success);

  void OnURLChange(const char *url);

  bool RecomputeReadaheadSize(uint64_t new_readahead_offset);
  ReadDataResult WaitForInterestingData(bool wakeup_transfer,
                                        int timeout_msec,
                                        int quit_event);
  typedef std::map<uint64_t, BufferChunk*> BufferMap;

  bool LatchTransferRange(uint64_t* start, uint64_t* end);

  void InternalResetStateVariables();

  bool SetupSeekIfNeeded();

  BufferMap buffers_;
  pthread_mutex_t buffers_lock_;
  PipeEvent interesting_data_evt_;
  IReadCacheOwner* owner_;
  A3ceCURLTransferEngine* transfer_;

  std::string base_url_;
  char* user_agent_;
  char* cookies_;
  KeyedVector<String8, String8>* headers_;
  uint32_t blocksize_;
  uint64_t file_len_;

  bool shutdown_now_;
  bool connection_should_pause_;
  bool has_waiter_;
  bool first_connect_;
  uint64_t waiter_interesting_start_;
  uint32_t waiter_interesting_len_;

  uint32_t total_ram_size_;
  uint64_t readahead_offset_start_;
  uint32_t readahead_ram_size_;

  uint64_t expected_next_read_offset_;
  uint64_t expected_next_write_offset_;
  uint64_t cur_http_start_offset_;
  uint64_t cur_http_end_offset_;
  bool     http_engine_needs_to_seek_;

  uint32_t bad_transfer_count_;

  uint32_t max_ram_;
  float readahead_percentage_;
};

}  // namespace android
#endif  // ANDROID_BUFFERS_H
