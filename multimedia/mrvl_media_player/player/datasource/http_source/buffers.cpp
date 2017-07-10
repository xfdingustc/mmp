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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/poll.h>

#include <map>

#include <utils/KeyedVector.h>
#include <utils/String8.h>

#include "http_source/buffers.h"
#include "http_source/a3ce_curl_transfer_engine.h"
#include "http_source/a3ce_curl_transfer_engine_rx_report.h"
#include "http_source/IReadCacheOwner.h"

namespace android {

const uint32_t  ReadCache::kMaxConsecutiveBadTransfers = 16;
const uint32_t  ReadCache::kMinMaxRAM     = (128 << 10);
const uint32_t  ReadCache::kMaxMaxRAM     = 0xFFFFFFFF;
const uint32_t  ReadCache::kDefaultMaxRAM = (16 << 20);
const long      ReadCache::kLSThreshBytesPerSec = 1l;
const long      ReadCache::kLSThreshDurationSecs = 3l;
const float ReadCache::kMinReadaheadPercentage = 0.0;
const float ReadCache::kMaxReadaheadPercentage = 1.0;
const float ReadCache::kDefaultReadaheadPercentage = 0.80;

static inline uint64_t GetMonotonicTicksNSec() {
  struct timespec ts;

  if (!clock_gettime(CLOCK_MONOTONIC, &ts)) {
    return (static_cast<uint64_t>(ts.tv_sec) * 1000000000ul) + ts.tv_nsec;
  } else {
    ALOGW("clock_gettime failed with lasterr %d in %s", errno, __FILE__);
  }

  return 0;
}

static inline uint64_t AbsDelta(uint64_t a, uint64_t b) {
  return (a > b) ? (a - b) : (b - a);
}

static inline bool RegionsOverlap(uint64_t a_start, uint64_t a_end,
                                  uint64_t b_start, uint64_t b_end) {
  return (a_start < b_end) && (b_start < a_end);
}

static inline bool ChunksOverlap(const BufferChunk& a, const BufferChunk& b) {
  uint64_t a_start = a.FileOffset();
  uint64_t a_end   = a_start + a.ChunkFilled();
  uint64_t b_start = b.FileOffset();
  uint64_t b_end   = b_start + b.ChunkFilled();

  return RegionsOverlap(a_start, a_end, b_start, b_end);
}

static inline bool RangeContainsOffset(uint64_t start, uint64_t end, uint64_t offset) {
  return ((start <= offset) && (end > offset));
}

static inline bool ChunkContainsOffset(const BufferChunk& chunk, uint64_t offset) {
  register uint64_t start = chunk.FileOffset();
  register uint64_t end   = start + chunk.ChunkFilled();
  return RangeContainsOffset(start, end, offset);
}

BufferChunk::BufferChunk(uint64_t file_offset, uint32_t chunk_size) {
  file_offset_  = file_offset;
  chunk_size_   = chunk_size;
  chunk_filled_ = 0;
  chunk_data_   = NULL;
}

BufferChunk::~BufferChunk() {
  delete[] chunk_data_;
}

bool BufferChunk::Allocate() {
  if (chunk_data_)
    return false;

  assert(chunk_size_);
  assert(!chunk_filled_);
  chunk_data_ = new uint8_t[chunk_size_];

  return (NULL != chunk_data_);
}

bool BufferChunk::AddToChunk(const uint8_t* data, uint32_t data_len) {
  if (!data || !chunk_data_)
    return false;

  const uint32_t space = Space();
  if (data_len > space)
    return false;

  memcpy(chunk_data_ + chunk_filled_, data, data_len);
  chunk_filled_ += data_len;

  return true;
}

ReadCache::ReadCache(IReadCacheOwner* owner) {
  pthread_mutex_init(&buffers_lock_, NULL);

  assert(owner);
  owner_ = owner;
  transfer_ = NULL;
  user_agent_ = NULL;
  cookies_ = NULL;
  headers_ = NULL;

  max_ram_              = kDefaultMaxRAM;
  readahead_percentage_ = kDefaultReadaheadPercentage;

  InternalResetStateVariables();
}

ReadCache::~ReadCache() {
  Reset();
  pthread_mutex_destroy(&buffers_lock_);
}

void ReadCache::Reset() {
  shutdown_now_ = true;

  // kill any active http transfer before proceeding.  This make sure that there
  // are no callbacks from the transfer engine are in flight while we reset the
  // rest of the engine.
  if (transfer_) {
    transfer_->Cleanup();
    delete transfer_;
    transfer_ = NULL;
  }

  pthread_mutex_lock(&buffers_lock_);

  // wipe the buffer map.
  for (BufferMap::iterator it = buffers_.begin();
       it != buffers_.end();
       ++it) {
    BufferChunk* c = it->second;
    assert(c);
    delete c;
  }
  buffers_.clear();
  pthread_mutex_unlock(&buffers_lock_);

  if (user_agent_) {
    free(user_agent_);
    user_agent_ = NULL;
  }

  if (cookies_) {
    free(cookies_);
    cookies_ = NULL;
  }

  if (headers_) {
    delete headers_;
    headers_ = NULL;
  }

  InternalResetStateVariables();

  interesting_data_evt_.setEvent();
}

void ReadCache::InternalResetStateVariables() {
  // The cache starts in the "shutdown" state.  Once it is setup, this flag will
  // be cleared.
  shutdown_now_ = true;
  first_connect_ = true;
  has_waiter_ = false;
  connection_should_pause_ = false;

  total_ram_size_ = 0;
  readahead_offset_start_ = 0;
  readahead_ram_size_ = 0;
  file_len_ = 0;
  bad_transfer_count_ = 0;

  expected_next_write_offset_ = 0;
  expected_next_read_offset_ = 0;
  cur_http_start_offset_ = 0;

  cur_http_end_offset_ = CURLTransferEngine::kReadUntilEnd;
  http_engine_needs_to_seek_ = false;

  base_url_.assign("");
}

void ReadCache::ClearCallbacks() {
  if (transfer_)
    transfer_->Cleanup();
}

bool ReadCache::Setup(const char* base_url,
                      uint32_t blocksize,
                      const char* user_agent,
                      const char* cookies,
                      const KeyedVector<String8, String8>* headers) {
  // If we are already running, we need to be reset before we can be set up
  // again.
  if (!shutdown_now_) {
    ALOGE("ReadCache::Setup failed; already setting up");
    return false;
  }

  if (!base_url || !blocksize) {
    ALOGE("ReadCache::Setup failed; bad URL or blocksize");
    return false;
  }

  assert(!base_url_.length());
  assert(!user_agent_);
  assert(!cookies_);
  assert(!total_ram_size_);
  assert(!readahead_offset_start_);
  assert(!readahead_ram_size_);
  assert(!buffers_.size());
  assert(first_connect_);
  assert(!file_len_);

  bool ret_val = false;
  interesting_data_evt_.clearPendingEvents();
  base_url_.assign(base_url);

  if (user_agent) {
    free(user_agent_);
    if (!(user_agent_ = strdup(user_agent)))
      goto bailout;
  }

  if (cookies) {
    free(cookies_);
    if (!(cookies_ = strdup(cookies)))
      goto bailout;
  }

  if (headers) {
    if (headers_)
      delete headers_;
    headers_ = new KeyedVector<String8, String8>();
    if (NULL == headers_)
      goto bailout;
    for (size_t i = 0; i < headers->size(); ++i)
      if (headers_->add(headers->keyAt(i), headers->valueAt(i)) < 0)
        goto bailout;
  }

  blocksize_ = blocksize;

  assert(!transfer_);
  transfer_ = new A3ceCURLTransferEngine(this, base_url_, blocksize_);
  if (!transfer_) {
    ALOGE("ReadCache::Setup failed; failed to allocate transfer engine");
    goto bailout;
  }

  if (user_agent_ && !transfer_->SetUserAgent(user_agent_)) {
    ALOGE("ReadCache::Setup failed; failed to set user agent");
    goto bailout;
  }

  if (cookies_ && !transfer_->SetCookies(cookies_)) {
    ALOGE("ReadCache::Setup failed; failed to set user agent");
    goto bailout;
  }

  if (headers_ && !transfer_->SetHeaders(headers_)) {
    ALOGE("ReadCache::Setup failed; failed to set headers");
    goto bailout;
  }

  // Set an agressive low speed threshold in order to quickly detect and respond
  // to connection stalls.
  transfer_->SetLowSpeedThreshold(ReadCache::kLSThreshBytesPerSec,
                                  ReadCache::kLSThreshDurationSecs);

  ret_val = transfer_->BeginTransfer();
  if (!ret_val)
    ALOGE("ReadCache::Setup failed; failed to begin transfer");

bailout:
  if (!ret_val)
    Reset();
  else
    shutdown_now_ = false;
  return ret_val;
}

void ReadCache::OnConnectionReady(bool success) {
  assert(transfer_);

  // We only need to report that the connection is ready the first time we
  // connect to the server.  Subsequent connections to the same URL can happen
  // as the reader seeks around in the cache, but they are of no interest to the
  // higher levels of the code.
  if (first_connect_) {
    // If this was a successful connection, cache important information (such as
    // the file length) so we can respond to queries from our owner regardless
    // of the state of the http connection.
    if (success && transfer_)
      file_len_ = transfer_->ContentLen();

    assert(owner_);
    owner_->OnConnectionReady(success);

    first_connect_ = false;
  }
}

void ReadCache::OnURLChange(const char *url) {
  assert(owner_);
  owner_->OnURLChange(url);
}

bool ReadCache::SetMaxRAM(uint32_t max_ram) {
  if ((max_ram < kMinMaxRAM) || (max_ram > kMaxMaxRAM))
    return false;

  if (max_ram < max_ram_) {
    // Setting max ram to a smaller value causes deadlock inside ReadCache API.
    // Since readahead_ram_size_ is set with a previous bigger value, the curl
    // engine thread will stay paused forever, because readahead_ram_size_ is
    // not recomputed once max size changed. If readahead_ram_size_ is
    // larger then then max_ram_ the curls operations are paused. In order to
    // recover readahead_ram_size_ has to get smaller then max_ram_, however
    // this is not possible since this will require a curl operation.
    return false;
  }

  // Hold the buffer lock while we update this parameter.  The buffer lock
  // protects the state of the cache.  While the cache is being updated, this
  // paramater will be used by the thread performing the update in order to
  // properly prune the cache.  By holding the lock while setting the parameter,
  // we can be sure that the update thread will observe the same value for this
  // setting for the duration of the update operation.
  pthread_mutex_lock(&buffers_lock_);
  max_ram_ = max_ram;
  pthread_mutex_unlock(&buffers_lock_);

  return true;
}

bool ReadCache::SetReadaheadPercentage(float readahead_percentage) {
  // Assert that the gstreamer framework enforced our minimum and maximum
  // values.
  if ((readahead_percentage < kMinReadaheadPercentage) ||
      (readahead_percentage > kMaxReadaheadPercentage))
    return false;

  // Hold the buffer lock while we update this parameter.  The buffer lock
  // protects the state of the cache.  While the cache is being updated, this
  // paramater will be used by the thread performing the update in order to
  // properly prune the cache.  By holding the lock while setting the parameter,
  // we can be sure that the update thread will observe the same value for this
  // setting for the duration of the update operation.
  pthread_mutex_lock(&buffers_lock_);
  readahead_percentage_ = readahead_percentage;
  pthread_mutex_unlock(&buffers_lock_);

  return true;
}

ReadCache::AddChunkResult ReadCache::AddChunk(BufferChunk* chunk) {
  if (!chunk)
    return ACRES_FATAL_ERROR;
  AddChunkResult ret_val = ACRES_FATAL_ERROR;
  pthread_mutex_lock(&buffers_lock_);

  // If the low level is pushing data into the cache level, then we know that a
  // transfer must have recently succeeded.  Go ahead and reset the bad transfer
  // count.
  bad_transfer_count_ = 0;

  uint64_t file_loc = chunk->FileOffset();

  std::pair<BufferMap::iterator, bool> result =
    buffers_.insert(BufferMap::value_type(file_loc, chunk));

  if (result.second)
    ret_val = ACRES_OK;

#ifdef DEBUG
  if (result.second)
  {
    BufferChunk* fatal_chunk = NULL;
    BufferMap::iterator prev = result.first;
    if (prev != buffers_.begin()) {
      --prev;
      assert((*prev).second);
      if (ChunksOverlap(*((*prev).second), *chunk))
        fatal_chunk = ((*prev).second);
    }

    if (!fatal_chunk) {
      BufferMap::iterator next = result.first;
      ++next;
      if (next != buffers_.end()) {
        assert((*next).second);
        if (ChunksOverlap(*((*next).second), *chunk))
          fatal_chunk = ((*next).second);
      }
    }

    if (fatal_chunk) {
      ALOGE("Fatal error while inserting chunk into map.  Insert chunk"
           " (%llx:%x) overlaps with pre-existing chunk at (%llx,%x)",
           chunk->FileOffset(), chunk->ChunkFilled(),
           fatal_chunk->FileOffset(), fatal_chunk->ChunkFilled());
      buffers_.erase(result.first);
      ret_val = ACRES_FATAL_ERROR;
    }
  }
#endif

  uint32_t chunk_size = chunk->ChunkFilled();
  total_ram_size_ += chunk_size;

  uint32_t max_readahead = MaxReadaheadBytes();
  if (RegionsOverlap(readahead_offset_start_,
                     readahead_offset_start_ + max_readahead,
                     file_loc,
                     file_loc + chunk_size)) {
    readahead_ram_size_ += chunk_size;
  }

  // If we are over the limit for our cache, its time to purge blocks until we
  // get back to the proper limit.
  while (total_ram_size_ > max_ram_) {
    // Select the block which is the farthest from the start of the readahead
    // region (and not in the readahead region) and purge it.  The farthest away
    // block will be either the begining or the end of the cache.

    // if total_ram_size > max_ram_, we have to have at least one block in the
    // cache
    assert(buffers_.size());

    BufferChunk* first_chunk = (buffers_.begin())->second;
    BufferChunk* last_chunk  = (buffers_.rbegin())->second;
    assert(first_chunk);
    assert(last_chunk);

    uint64_t first_chunk_start =  first_chunk->FileOffset();
    uint64_t first_chunk_end   =  first_chunk_start + first_chunk->ChunkFilled();
    uint64_t last_chunk_start  =  last_chunk->FileOffset();
    uint64_t last_chunk_end    =  last_chunk_start +  last_chunk->ChunkFilled();

    uint64_t first_delta = AbsDelta(readahead_offset_start_, first_chunk_start);
    uint64_t last_delta  = AbsDelta(readahead_offset_start_, last_chunk_start);

    // Never trim the chunk we just added.  If the chunk farthest out in front
    // of us is the chunk we just added, we would rather either trim from the
    // buffer behind us, or not trim at all and overflow the budget a little
    // bit.  We will signal the need to pause down the the low level soon
    // enough.
    if (last_chunk == chunk)
      last_delta = 0;

    if ((last_delta > first_delta) &&
        !RegionsOverlap(readahead_offset_start_,
                        readahead_offset_start_ + max_readahead,
                        last_chunk_start,
                        last_chunk_end)) {
      // purge the chunk at end of the cache.
      buffers_.erase(--buffers_.end());
      total_ram_size_ -= last_chunk->ChunkFilled();
      delete last_chunk;
    } else {
        if (!RegionsOverlap(readahead_offset_start_,
                            readahead_offset_start_ + max_readahead,
                            first_chunk_start,
                            first_chunk_end)) {
          // purge the chunk at start of the cache.
          buffers_.erase(buffers_.begin());
          total_ram_size_ -= first_chunk->ChunkFilled();
          delete first_chunk;
        } else {
          ALOGW("ReadCache size %d exceeds limit %d but all blocks appear"
               " to be in the readahead cache so there is nothing to purge."
               " Consider reducing the readahead percentage to something below"
               " 100%%", total_ram_size_, max_ram_);
          break;
        }
    }
  }

  // If we are at the readahead limit, we need to signal to the http thread
  // that it needs to pause.
  if ((ret_val == ACRES_OK) && (readahead_ram_size_ >= max_readahead)) {
    ret_val = ACRES_PAUSE_CONNECTION;
    connection_should_pause_ = true;
  }

  // If there is someone waiting for the data we just received, go ahead and let
  // them know that it has arrived.
  if (has_waiter_ && ChunkContainsOffset(*chunk, waiter_interesting_start_))
    interesting_data_evt_.setEvent();

  // Update where we expect the underlying transfer engine to write to next.
  expected_next_write_offset_ = chunk->FileOffset() + chunk->ChunkFilled();

  // If we don't already have a different place for the transfer engine to seek
  // to, and the next data we expect the transfer engine to read is the end of
  // the region we told the engine to transfer in the first place, then we need
  // to find the next chunk of the cache (if any) which the transfer needs to
  // fill in.
  if (!http_engine_needs_to_seek_) {
    assert(expected_next_write_offset_ <= cur_http_end_offset_);
    if (expected_next_write_offset_ == cur_http_end_offset_) {
      cur_http_start_offset_ = expected_next_write_offset_;
      BufferMap::iterator it = result.first;
      while ((++it) != buffers_.end()) {
        BufferChunk* c = it->second;
        assert(c);
        uint64_t chunk_end = c->FileOffset() + c->ChunkFilled();
        if (c->FileOffset() != cur_http_start_offset_) {
          cur_http_end_offset_ = chunk_end;
          break;
        } else {
          cur_http_start_offset_ = chunk_end;
        }
      }

      if (cur_http_start_offset_ < file_len_) {
        ret_val = ACRES_SEEK_CONNECTION;
        http_engine_needs_to_seek_ = true;
        connection_should_pause_ = true;
        expected_next_write_offset_ = cur_http_start_offset_;
        if (it == buffers_.end())
          cur_http_end_offset_ = CURLTransferEngine::kReadUntilEnd;
        assert(cur_http_end_offset_ > cur_http_start_offset_);
      }
    }
  }

  pthread_mutex_unlock(&buffers_lock_);
  return ret_val;
}

ReadCache::ReadDataResult ReadCache::WaitForInterestingData(
    bool wakeup_transfer,
    int timeout_msec,
    int quit_event) {
  // TODO(johngro) : assert we are holding the buffers_lock when we enter this
  // function.

  // If we are waiting for data, it is either because it is going to be the next
  // data that the underlying engine reads, or because we need to see the
  // transfer engine to a new offset in the file.  Figure out if we need to seek
  // or not, and if we do, make certain the underlying transfer knows.
  uint64_t interesting_block_start =
    RoundDownToBlockOffset(waiter_interesting_start_);
  if (interesting_block_start != expected_next_write_offset_) {
    // looks like we need to seek.  Figure out where the end of the next range
    // is for the transfer engine.
    cur_http_start_offset_ = interesting_block_start;
    expected_next_write_offset_ = interesting_block_start;
    http_engine_needs_to_seek_ = true;
    connection_should_pause_ = false;
    wakeup_transfer = true;
    BufferMap::iterator it = buffers_.upper_bound(cur_http_start_offset_);
    if (it != buffers_.end())
      cur_http_end_offset_ = it->second->FileOffset();
    else
      cur_http_end_offset_ = CURLTransferEngine::kReadUntilEnd;

    ALOGI("Cache miss, next read op is %lld - %lld",
         cur_http_start_offset_,
         cur_http_end_offset_);
  }

  if (!timeout_msec)
    return RDRES_TIMEOUT;

  struct pollfd poll_structs[2];
  int number_of_events = 1;

  poll_structs[0].fd = interesting_data_evt_.getWakeupHandle();
  poll_structs[0].events = POLLIN;
  poll_structs[0].revents = 0;

  if (quit_event >= 0) {
    poll_structs[1].fd = quit_event;
    poll_structs[1].events = POLLIN;
    poll_structs[1].revents = 0;
    number_of_events++;
  }

  int poll_res;
  pthread_mutex_unlock(&buffers_lock_);
  if (wakeup_transfer && transfer_)
    transfer_->WakeupTransfer();
  poll_res = poll(poll_structs, number_of_events, timeout_msec);
  pthread_mutex_lock(&buffers_lock_);

  interesting_data_evt_.clearPendingEvents();

  // A negative poll result indicates that something Very Bad happened
  // internally.
  if (poll_res < 0) {
    ALOGE("Fatal error in %s while waiting for poll result (errno = %d)",
         __PRETTY_FUNCTION__, errno);
    return RDRES_FATAL_ERROR;
  }

  // A poll result of zero indicates a timeout
  if (!poll_res)
    return RDRES_TIMEOUT;

  // If the quit event was defined and fired, then indicate that it was the
  // wakeup cause.
  if ((quit_event >= 0) && (poll_structs[1].revents != 0))
    return RDRES_EVENT_SIGNALLED;

  // Looks like we woke up because some interesting data came in.
  return RDRES_OK;
}

ReadCache::ReadDataResult ReadCache::ReadData(uint8_t* tgt,
                                              uint64_t start,
                                              uint32_t len,
                                              size_t* amt_read,
                                              int timeout_msec,
                                              int quit_event) {
  if (!amt_read)
    return RDRES_FATAL_ERROR;

  // NULL tgt means error
  if (!tgt) {
    *amt_read = -1;
    return RDRES_FATAL_ERROR;
  }

  // Figure out if this is an attempt to read past the end of the file.  If it
  // is, then limit the length of the read.  If this would result in a negative
  // length read, then return OK and indicate a read size of -1 (which is what
  // would happen if you attempt to read past the end of a file using normal
  // file i/o)
  int64_t past_end_delta = ((start + len) - file_len_);
  if ((past_end_delta > 0) && transfer_ && transfer_->HasContentLen()){
    if (past_end_delta >= len) {
      *amt_read = -1;
      return RDRES_OK;
    }
    len -= static_cast<uint32_t>(past_end_delta);
  }

  if (!len) {
    *amt_read = 0;
    return RDRES_OK;
  }

  pthread_mutex_lock(&buffers_lock_);
  if (has_waiter_) {
    ALOGE("Cannot currently have multiple threads reading data at the"
         " same time in the same ReadCache.");
    pthread_mutex_unlock(&buffers_lock_);
    *amt_read = -1;
    return RDRES_FATAL_ERROR;
  }

  uint64_t read_op_start = 0;
  uint64_t last_expected_read;
  bool need_wakeup = false;
  ReadDataResult ret_val = RDRES_OK;

  *amt_read = 0;
  has_waiter_ = true;
  waiter_interesting_start_ = start;
  waiter_interesting_len_   = len;

  if (!IsInfiniteTimeout(timeout_msec))
    read_op_start = GetMonotonicTicksNSec();

  while (waiter_interesting_len_ && !shutdown_now_) {
    // If the last underlying transfer finished with an error, and we don't have
    // a pending seek already in the works, try to set up a new transfer in
    // order to pick up where we left off.
    bool abort_if_need_to_wait = false;
    if (transfer_ &&
        transfer_->TransferFinished() &&
       (transfer_->TransferResult() != CURLE_OK) &&
        !http_engine_needs_to_seek_) {

      if (bad_transfer_count_ < kMaxConsecutiveBadTransfers) {
        // Invalidate expected_next_write_offset; we no longer expect the
        // underlying CURL transfer to write anywhere anymore.
        expected_next_write_offset_ = -1;
        ++bad_transfer_count_;
        ALOGW("Last transfer finished with an error (%d), forcing seek op.",
             transfer_->TransferResult());
        SetupSeekIfNeeded();
        if (transfer_)
          transfer_->WakeupTransfer();
      } else {
        // Things have gotten pretty bad apparently.  We have maxed out our
        // consecutive connection failures.  If we can satisfy this read request
        // from what is in the cache, then go ahead and do so.  If we would need
        // to block and wait for data, we need to abort with an error since the
        // underlying connection seems to be perma-hosed at this point.
        abort_if_need_to_wait = true;
      }
    }

    BufferMap::iterator it;
    it = buffers_.lower_bound(waiter_interesting_start_);

    // Because of the lower_bound, we know that the start of this block is
    // greater than or equal to the start of the interesting region.  If it is
    // equal to, then great, go ahead and use it.  If it is greater than, back
    // up to the previous block in the list (if any) which must start strictly
    // before the interesting region start.  If we can't back up because we are
    // at the begining of the cache, we need to go to sleep and wait for more
    // data.
    assert((buffers_.end() == it) || it->second);
    if ((buffers_.end() == it) ||
        (it->second->FileOffset() > waiter_interesting_start_)) {
      if (buffers_.begin() != it)
        --it;
      else
        it = buffers_.end();
    }

    while (!shutdown_now_ &&
           (it != buffers_.end()) &&
           waiter_interesting_len_) {
      BufferChunk* buf = it->second;

      assert(buf);
      uint64_t buf_start = buf->FileOffset();
      uint32_t buf_len   = buf->ChunkFilled();

      // If the next byte we want to read is not in this chunk, then we need to
      // wait for new data.  Break out of the while loop so we can do so.
      if (!RangeContainsOffset(buf_start,
                               buf_start + buf_len,
                               waiter_interesting_start_))
        break;

      // At this point, thanks to the call to lower_bound above along with the
      // check which follows it, it should be possible for any intersecting
      // block to start after the start of the interesting region.
      assert(buf_start <= waiter_interesting_start_);
      uint64_t buf_offset = waiter_interesting_start_ - buf_start;

      // Because these regions intersect, it should be impossible for the
      // buf_offset to be greater than or equal to the filled amt of the buffer
      // chunk.
      assert(buf_offset < buf_len);
      uint32_t todo = buf_len - static_cast<uint32_t>(buf_offset);
      if (todo > waiter_interesting_len_)
        todo = waiter_interesting_len_;

      // Go ahead and copy our data.
      memcpy(tgt + *amt_read,
             buf->ChunkData() + static_cast<uint32_t>(buf_offset),
             todo);

      waiter_interesting_start_ += todo;
      waiter_interesting_len_   -= todo;
      *amt_read                 += todo;
      ++it;
    }

    if (shutdown_now_)
      break;

    if(waiter_interesting_len_) {
      if(abort_if_need_to_wait) {
        ALOGE("Max consecutive tranfer failures reached (%d). Aborting the"
             " read op with a failure.", bad_transfer_count_);
        *amt_read = -1;
        ret_val = RDRES_FATAL_ERROR;
        goto bailout;
      }

      uint32_t effective_timeout = timeout_msec;
      if (!IsInfiniteTimeout(effective_timeout)) {
        uint64_t read_op_delta = GetMonotonicTicksNSec() - read_op_start;
        read_op_delta /= 1000000;

        if (read_op_delta < effective_timeout)
          effective_timeout -= static_cast<uint32_t>(read_op_delta);
        else
          effective_timeout = 0;
      }

      bool need_wakeup;

      need_wakeup = RecomputeReadaheadSize(start + *amt_read);
      ret_val     = WaitForInterestingData(need_wakeup,
                                           effective_timeout,
                                           quit_event);
      if (RDRES_OK != ret_val)
        waiter_interesting_len_ = 0;
    }
  }

  need_wakeup = RecomputeReadaheadSize(start + *amt_read);

  // Update the next place we expect to read from.
  last_expected_read = expected_next_read_offset_;
  expected_next_read_offset_ = start + *amt_read;

  // If this read operation was not where we expected it to be then we need to
  // make certain that the transfer engine is reading from the correct place
  // right now as well.
  if (start != last_expected_read) {
    if (SetupSeekIfNeeded())
      need_wakeup = true;
  }

  has_waiter_ = false;

bailout:
  pthread_mutex_unlock(&buffers_lock_);

  if (need_wakeup && transfer_)
    transfer_->WakeupTransfer();

  return ret_val;
}

bool ReadCache::SetupSeekIfNeeded() {
  bool need_wakeup = false;

  // We need to look at the readahead region of cache and determine where the
  // next gap is (IOW the end of the contiguous region of the readahead
  // buffer).
  uint64_t contig_region_end =
    RoundDownToBlockOffset(expected_next_read_offset_);

  BufferMap::iterator it = buffers_.lower_bound(contig_region_end);
  while (it != buffers_.end()) {
    BufferChunk* c = it->second;
    assert(c);
    if (c->FileOffset() != contig_region_end)
      break;
    contig_region_end = c->FileOffset() + c->ChunkFilled();
    ++it;
  }

  if (expected_next_write_offset_ != contig_region_end) {
    // Looks like we need to signal a seek.
    connection_should_pause_ = false;
    http_engine_needs_to_seek_ = true;
    need_wakeup = true;
    cur_http_start_offset_ = contig_region_end;
    expected_next_write_offset_ = contig_region_end;
    if (it != buffers_.end())
      cur_http_end_offset_ = it->second->FileOffset();
    else
      cur_http_end_offset_ = CURLTransferEngine::kReadUntilEnd;
  }

  return need_wakeup;
}

bool ReadCache::RecomputeReadaheadSize(uint64_t new_readahead_offset) {
  // TODO(johngro) : assert we are holding the buffers_lock when we enter this
  // function.

  // Readahead regions can only start at a block boundary.  Round down the
  // offset passed in to enforce this.
  new_readahead_offset = RoundDownToBlockOffset(new_readahead_offset);

  // if the current readahead offset is the same as the new offset, there is
  // nothing to do.
  if (readahead_offset_start_ == new_readahead_offset)
    return false;

  // The region has moved, compute the range which has moved out of the
  // readahead region as well as the range which is now in the region.  Then use
  // these ranges to update the amt of ram which is in the readahead region.
  uint64_t purge_start, purge_end;
  uint64_t add_start, add_end;
  uint32_t max_readahead = MaxReadaheadBytes();
  uint64_t new_readahead_end = new_readahead_offset + max_readahead;
  uint64_t old_readahead_end = readahead_offset_start_ + max_readahead;
  bool do_purge = true;

  // Special case: If the regions are totally disjoint, then there is no need to
  // do the purge.  Just set the amt of used ram to 0 and then add in the data
  // in the new region.
  // TODO(johngro) : Actually, the break point for work to be done here should
  // be when the regions overlap by less than 1/2 max_readahead.  At that point
  // in time, it should be cheaper to simply set readahead_ram_size_ to 0 and
  // just walk the entire new region.  At some point, I need to come back and
  // write that logic in instead of simply testing for any overlap.
  if (!RegionsOverlap(readahead_offset_start_,
                      old_readahead_end,
                      new_readahead_offset,
                      new_readahead_end)) {
    do_purge = false;
    add_start = new_readahead_offset;
    add_end = new_readahead_end;
    readahead_ram_size_ = 0;
  } else {
    if (new_readahead_offset < readahead_offset_start_) {
      add_start   = new_readahead_offset;
      add_end     = readahead_offset_start_;
      purge_start = add_start + max_readahead;
      purge_end   = add_end + max_readahead;
    } else {
      purge_start = readahead_offset_start_;
      purge_end   = new_readahead_offset;
      add_start   = purge_start + max_readahead;
      add_end     = purge_end + max_readahead;
    }
  }

  // At this point, we know what needs to be purged and what needs to be added.
  // Go ahead and do it.
  if (do_purge && (purge_end > purge_start)) {
    BufferMap::iterator it = buffers_.upper_bound(purge_start);
    if ((it != buffers_.begin()))
      --it;

    bool first_time = true;
    while (it != buffers_.end()) {
      BufferChunk* chunk = it->second;
      assert(chunk);
      if (chunk->FileOffset() >= purge_end)
        break;

      register uint64_t chunk_start = chunk->FileOffset();
      register uint64_t chunk_end   = chunk->FileOffset() + chunk->ChunkFilled();

      // Its possible that the first buffer we look at was not in the old
      // readahead region at all.  All subsequent buffers we examine at this
      // point must be.
      if (!first_time || RegionsOverlap(readahead_offset_start_,
                                        old_readahead_end,
                                        chunk_start, chunk_end)) {
        first_time = false;

        // If the chunk is not in the new readahead region, then remove it from
        // the readahead_ram_size_ total.  Note: only the last buffer we examine
        // has a chance of still being part of the new readahead range.
        if (!RegionsOverlap(new_readahead_offset, new_readahead_end,
                            chunk->FileOffset(),
                            chunk->FileOffset() + chunk->ChunkFilled())) {
          // purge the chunk from the total
          assert(readahead_ram_size_ >= chunk->ChunkFilled());
          readahead_ram_size_ -= chunk->ChunkFilled();
        }
      }

      ++it;
    }
  }

  // Now add the size of the buffers in the add region back into the total.
  if (add_end > add_start) {
    BufferMap::iterator it = buffers_.upper_bound(add_start);
    if ((it != buffers_.begin()))
      --it;

    while (it != buffers_.end()) {
      BufferChunk* chunk = it->second;
      assert(chunk);
      if (chunk->FileOffset() >= add_end)
        break;

      register uint64_t chunk_start = chunk->FileOffset();
      register uint64_t chunk_end   = chunk->FileOffset() + chunk->ChunkFilled();

      // If this chunk was not in the old readahead region, but is in the new
      // readahead region, add its size to the total.
      if ((!RegionsOverlap(readahead_offset_start_, old_readahead_end,
                           chunk->FileOffset(),
                           chunk->FileOffset() + chunk->ChunkFilled())) &&
          (RegionsOverlap(new_readahead_offset, new_readahead_end,
                          chunk->FileOffset(),
                          chunk->FileOffset() + chunk->ChunkFilled()))) {
        readahead_ram_size_ += chunk->ChunkFilled();
      }

      ++it;
    }
  }

  // OK, our accounting is done.  Update the new readahead_offset_start_
  readahead_offset_start_ = new_readahead_offset;

  // if we now have space in our readahead region, be certain to let the http
  // engine know.
  if (connection_should_pause_ && (readahead_ram_size_ < max_readahead)) {
    connection_should_pause_ = false;
    return true;
  }

  return false;
}

bool ReadCache::LatchTransferRange(uint64_t* start, uint64_t* end) {
  assert(start);
  assert(end);
  pthread_mutex_lock(&buffers_lock_);

  bool ret_val = http_engine_needs_to_seek_;
  *start = cur_http_start_offset_;
  *end   = cur_http_end_offset_;
  http_engine_needs_to_seek_ = false;

  pthread_mutex_unlock(&buffers_lock_);
  return ret_val;
}

// In theory, if the bookkeeping is accurate, this should just be the distance
// between the expected next read operation and the expected next write
// operation.
uint64_t ReadCache::GetBufferedBytes() {
  pthread_mutex_lock(&buffers_lock_);

  uint64_t ret_val = 0;

  if ((static_cast<uint64_t>(-1)  != expected_next_write_offset_) &&
      (expected_next_read_offset_ <= expected_next_write_offset_))
    ret_val = (expected_next_write_offset_ - expected_next_read_offset_);

  pthread_mutex_unlock(&buffers_lock_);
  return ret_val;
}

void ReadCache::SendRXReport(const A3ceCURLTransferEngineRXReport& report) {
  if (!owner_)
    return;

  pthread_mutex_lock(&buffers_lock_);
  uint32_t readahead = readahead_ram_size_;
  uint32_t max_readahead = MaxReadaheadBytes();
  pthread_mutex_unlock(&buffers_lock_);

  // It is technically possible for the readahead amt to be greater than the max
  // readahead amt temporarily.  When this happens, adjust the reported max
  // readahead to prevent confusion at the application level.
  if (max_readahead < readahead)
    max_readahead = readahead;

  // TODO(johngro) : create a gstreamer message which contains the bitrate
  // report data augmented with the current status of the readahead buffer and
  // then post it to our owner's message bus.
  ALOGI("Readahead    = %d/%d\n", readahead, max_readahead);
  ALOGI("Realtime bps = %d", report.RecentRealtimeByterate() << 3);
  ALOGI("Windowed bps = %d (winsize = %.3lf mSec)",
       report.RecentWindowedByterate() << 3,
       static_cast<double>(report.RecentWindowSize()) / 1000.0);
}

}  // namespace android