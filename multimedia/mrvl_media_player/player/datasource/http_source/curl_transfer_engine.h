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


#ifndef ANDROID_CURL_TRANSFER_ENGINE_H
#define ANDROID_CURL_TRANSFER_ENGINE_H

#include <stdint.h>

#include <queue>
#include <set>
#include <list>
#include <string>

#include <curl/curl.h>
//#include <utils/KeyedVector.h>
//#include <utils/String8.h>

#include "thread_utils.h"

// Some additional error codes we use in addition to the existing CURL error
// code.
#define CURLE_BAD_POINTER ((CURLcode)(CURLE_ALREADY_COMPLETE + 1000))
#define CURLE_TRANSFER_STILL_RUNNING ((CURLcode)(CURLE_ALREADY_COMPLETE + 1001))
#define CURLE_DESERIALIZATION_FAILED ((CURLcode)(CURLE_ALREADY_COMPLETE + 1002))

namespace android {
class String8;
template <typename KEY, typename VALUE> class KeyedVector;
}

namespace android {

using android::String8;
using android::KeyedVector;

// libCURL recomends that when you are using their multi interface and there are
// any ongoing transfers, that you wake up at least once every couple of seconds
// and call curl_multi_perform in order to ensure that libCURLs timeout behavior
// functions properly.  Currently, we have this timeout set to once every 200
// mSec.  Eventually, it should be made into a configurable property of the
// engine.
// TODO(johngro): Make this a configurable property.
extern const int kMinCURLPollPeriod;

// kVerboseCURLOps control whether or not libCURL dumps diagnostic information
// to stderr while processing transfer operations.  By default it is set to 0
// and should only be turned on while debugging.
extern const int kVerboseCURLOps;

// kDefaultConnectTimeoutInSeconds sets the maximum amount of time that CURL
// will wait until it connects to the server.  Currently this is set to 120
// seconds.
extern const long kDefaultConnectTimeoutInSeconds;

// kDefaultReadTimeoutInSeconds sets the maxumum amount of time that CURL
// will wait until it receives data.  This includes the amount of time for
// connection on the first read (so may override the connection timeout).
extern const long kDefaultReadTimeoutInSeconds;

// Defaults for the low speed threshold settings.  See the comments around
// SetLowSpeedThreshold for details.
extern const long kDefaultLSThreshBytesPerSec;
extern const long kDefaultLSThreshDurationSecs;

// A CURLTransferEngine is a class which makes use of libCURL to marshal
// protocol buffers to and from a web service endpoint.  Each CURLTransferEngine
// represents a single HTTP request/response, however multiple transfer engines
// may be instantiated at the same time in order to conduct multiple requests in
// parallel.  Transfers are explicitly asynchronous; no synchronous interface is
// provided.  No call to an instance of a CURLTransferEngine should ever block
// on the network.  At the end of a transfer, the CURLTransferEngine instance
// can either be destroyed (closing the connection to the server) or reset and
// reused, which will cause the underlying CURL code to attempt to reuse the
// server connection (if it can).
//
// CURLTransferEngines use the curl_multi interfaces (see the documentation for
// libCURL at http://curl.haxx.se for details) in order to achieve asynchronous
// and parallel requests.  Because of this, there are a number of shared static
// members of the class which manage the curl_multi handle which all of the
// curl_easy handles share.  No threads are used in order actually pump the low
// level CURL transfers.  Instead, the data pump needs to be driven by some
// other engine thread in the sytem.  In the case of the mosaic log engine, this
// is the log engine's work thread.  Applications may simply poll this pump if
// they choose to, but they are strongly encouraged to make use of the support
// for the posix fd_set/select pattern provided by the static GetFDSets method.
//
// Thread safety for the CURLTransferEngine is handled internally only for the
// following operations.
// - Creating a new transfer operation.
// - Destroying an existing transfer operation (at any time, include while the
//   transfer is in progress).
// - Fetching FD sets to use with select(2)
// - Pumping data via the PumpData() call.
//
// In addition, parallel operations on different instances of a
// CURLTransferEngine are inherently thread-safe.  For instance, it is OK for
// two threads to be calling AddToTransfer at the same time on different
// CURLTransferEngine instances.
//
// For all other operations it is assumed that the caller manages the safety of
// multithreaded operations.  In other words, it is up to the caller to
// serialize access between multiple threads to a single CURLTransferEngine
// instance.  It is particularly important to note that once StartTransfer has
// been called but before the transfer has finished, only the following
// operations are permitted on a CURLTransferEngine instance.
// - Calls to the CURLTransferEngine's destructor (either by going out of scope
// or by using the delete operator).  Destroying a CURLTransferEngine which is
// in the process of transfering data will abort the transfer and close the
// connection to the server.
// - Queries to the status flags (TransferStarted and TransferFinished)
//
// Finally, internal to the CURLTransfer engine are a set of classes used as a
// bridge between CURL's desired buffer model and the proto2
// serialization/deserialization buffer model.
//
// BufferChain represents a linked list of buffers of parameterized size as well
// as state variables used to keep track of a read position in the buffer.
// These read pointer state variables are used either by the CURL callback
// interface when sending data to the server, or by the proto2 callback
// interface when deserializing a response.
//
// RXBufferChain is an instance of a BufferChain which implements the protobuf
// ZeroCopyInputStream interface and is used to receive data from the network
// and provide it to the protobuf callback interface for deserialization.
//
// TXBufferChain is an instance of a BufferChain which implements the protobuf
// ZeroCopyOutputStream interface and is used to receive data from the protobuf
// callback interface for serialization and make it availble to the CURL
// callback interface for transmission on the network.
class CURLTransferEngine;
typedef std::queue<CURLTransferEngine*> EngineQueue;
typedef std::set<CURLTransferEngine*> EngineSet;
typedef std::list<CURLTransferEngine*> EngineList;
class CURLTransferEngine {
 public:
  static const uint64_t kReadUntilEnd;

  // The CURLTransferEngine constructor begins the process of setting up an HTTP
  // request.  It takes the url destination of the request as well as the chunk
  // size to use when creating buffer chains.  Requests which are expected to
  // use small request bodies or which expect to see small responses coming back
  // from servers should use small chunk sizes.  Larger requests should use
  // larger chunk sizes, up to a page in size.  Chunk sizes exceeding a page in
  // size provide minimal extra efficiency and could cause problems in a highly
  // fragmented heap and therefore should probably be avoided.
  CURLTransferEngine(const std::string& url, size_t new_chunk_size);

  // The destructor of a CURLTransferEngine closes the connection to the server
  // and cleans up after the request.  Generally, CURLTransferEngines should not
  // be kept lying around after a transfer is complete unless there is going to
  // be another request to the same server which needs to be performed almost
  // immediately.
  virtual ~CURLTransferEngine();

  // This must be called before deleting any engine which has been started.
  void Cleanup();

  // The following static methods are the interface to the code which manages
  // the shared curl_multi handle and which implements the data pump.
  // Initialize must be called once before instantiating any
  // CURLTransferEngines.  Shutdown should be called when the application is
  // finished using the CURLTransferEngine system.
  static bool Initialize();
  static void Shutdown();

  // Provide a new URL and reset all of the internal state for a new request.
  // From a functionality standpoint, this is almost identical to the
  // constructor, the primary difference being that ResetForNewRequest lets a
  // CURLTransferEngine reuse the server connection from the request immediately
  // prior to this one, if possible.
  virtual void ResetForNewRequest(const std::string& url);

  // Set and Get the User-Agent string to use on the connection.  Setting to
  // NULL (the default) will cause no User-Agent string to be tranmitted.  The
  // User-Agent cannot be set after the transfer has started.
  bool SetUserAgent(const char* user_agent);
  const char* GetUserAgent() const { return user_agent_; }

  // Set and Get the Cookies string to use on the connection.  Setting to NULL
  // (the default) will cause no Cookies string to be tranmitted.  The Cookies
  // cannot be set after the transfer has started.
  bool SetCookies(const char* cookies);
  const char* GetCookies() const { return cookies_; }

  // Set and Get the headers to use on the connection.  Setting to NULL
  // (the default) will cause no additional header string to be tranmitted.
  // The headers cannot be set after the transfer has started.
  bool SetHeaders(const KeyedVector<String8, String8>* headers);
  struct curl_slist* GetHeaders() const { return headers_; }

  // Set and Get the maximum number of HTTP redirects to follow.  The default is
  // 5. The maximum number of redirects cannot be set after the transfer has
  // started.
  bool SetMaxRedirects(long max_redirects);
  long GetMaxRedirects() const { return max_redirects_; }

  // Set the range of the get operation to be fetched.  Pass kReadUntilEnd to
  // the end_loc parameter to read from the starting location to the end of the
  // file (no matter how long it is).  These parameters can only be set after
  // the engine is created/reset, but before the transfer has begun.  By
  // default, the range [0, EndOfFile] will be fetched.
  bool     SetRXRange(uint64_t start_loc, uint64_t end_loc);
  uint64_t GetRXStart() const { return start_offset_; }
  uint64_t GetRXEnd  () const { return end_offset_; }

  // Accessor functions for internal state variables.  TransferStarted and
  // TransferFinished return the flags which indicate whether BeginTransfer has
  // been called and if the transfer has completed, respectively.
  // TransferResult returns the CURL status code indicating the result of the
  // transfer operation.
  bool TransferStarted() const { return transfer_started_; }
  bool TransferFinished() const { return transfer_finished_; }
  CURLcode TransferResult() const { return transfer_result_; }

  // GetCURLHandle gives a user of the application access to the curl_easy
  // handle in order to allow them to customize some of the aspects of the
  // request (things like setting HTTP headers, and so on).
  CURL* GetCURLHandle() const { return curl_easy_handle_; }

  // Get and set the maximum amount of time that CURL will wait while
  // connecting to a server.  A value of 0 indicates no timeout.
  void SetConnectionTimeout(int seconds) { connect_timeout_ = seconds; }
  int GetConnectionTimeout() { return connect_timeout_; }

  // Get and set the maximum amount of time that CURL will wait for the complete
  // transfer from the the server to complete.  On the first read attempt, the
  // timeout will be the lesser of this value and the connection timeout.  A
  // value of 0 indicates no read timeout.
  void SetReadTimeout(int seconds) { read_timeout_ = seconds; }
  int GetReadTimeout() { return read_timeout_; }

  // Get and set the low speed connection threshold for the CURL transfer.
  // During the data transfer phase of the connection, if the average transfer
  // rate falls below bytes_per_sec bytes/sec for duration_seconds concecutive
  // seconds the transfer will abort with a CURLE_OPERATION_TIMEDOUT error code.
  // Set either one of these parameters to 0 to disable the low speed threshold.
  // The low speed threshold defaults to disabled.
  void SetLowSpeedThreshold(long bytes_per_sec, long duration_seconds) {
    ls_bytes_per_sec_ = bytes_per_sec;
    ls_duration_secs_ = duration_seconds;
  }
  long GetLowSpeedThresholdBytesPerSec() const { return ls_bytes_per_sec_; }
  long GetLowSpeedThresholdDurationSecs() const { return ls_duration_secs_; }

  // BeginTransfer actually starts a transfer by adding the transfer's
  // curl_easy_handle to the common curl_multi_handle.  It should be called
  // once, and only after all of the setup for the transfer operation has been
  // completed.
  bool BeginTransfer();

  // WakeupTransfer signals the work thread that a particular transfer needs
  // attention.  Typically, it is used to wake up a paused transfer so it may
  // resume.
  void WakeupTransfer();

 protected:
  virtual size_t RXFunction(void* ptr, size_t size, size_t nmemb);
  virtual size_t TXFunction(void* ptr, size_t size, size_t nmemb);
  virtual size_t OnHeaderRX(void* ptr, size_t size, size_t nmemb);
  virtual void   OnURLChange(const char *url);
  virtual void   OnTransferFinished();
  virtual size_t PostSize();

  // AttentionCallback is an internal callback which is called by the work
  // thread when an engine has been scheduled for service by a client's call to
  // WakeupTransfer.  Usually, an engine is scheduled for service because it is
  // time to resume a previously paused transfer or it is time to seek to a new
  // segment of the file.
  virtual void AttentionCallback();

  // An internal version of WakeupTransfer meant to be called by descendent
  // classes during one of the CURL callbacks.  The only difference between this
  // method and the public WakeupTransfer is that this method assumes that you
  // are already holding the work thread lock.
  void InternalWakeupTransfer();

  // TimerCallback is an internal callback which is called by the work thread
  // when an engine has been scheduled for service by a client's call to
  // ScheduleTimerCallback.
  virtual void TimerCallback();

  // ScheduleTimerCallback is used by derrived classes to schedule a timed
  // callback at the time indicated by tick_count_wakeup_time on the absolute
  // GetTickCount timeline.  Clients (as determined by their this pointer) may
  // have only pending timer callback at a time.  Successive calls to
  // ScheduleTimerCallback will replace any already-scheduled callback.  Passing
  // kInvalidTickCountTime as the wakeup time will cancel any previous callback.
  //
  // The lock_work_thread parameter control whether or not the work thread lock
  // is obtained while scheduling the timer.  It should be set to false whenever
  // ScheduleTimerCallback is called from within the context of an engine
  // callback and set to true otherwise.
  void ScheduleTimerCallback(uint64_t tick_count_wakeup_time,
                             bool lock_work_thread);

  size_t NewChunkSize() const { return new_chunk_size_; }
  CURL*  EasyHandle() const { return curl_easy_handle_; }

  // GetTickCount provides descendents of this class access to an absolute
  // 64 bit timeline which is zeroed at static initialization time.  The
  // timeline's units are in microseconds which should give the total clock a
  // period of ~584k years making it impossible for the clock to ever roll over
  // during use.
  //
  // The GetTickCount timeline is used by the TimeoutCallback system to
  // coordinate timeout callbacks on an absolute schedule.
  //
  static uint64_t GetTickCount() {
    struct timeval now_time;
    if (0 != gettimeofday(&now_time, NULL))
      return kInvalidTickCountTime;

    uint64_t ret_val = (static_cast<uint64_t>(now_time.tv_sec) * 1000000) +
                        static_cast<uint64_t>(now_time.tv_usec) -
                        get_tick_count_base_;

    return ret_val;
  }

  static const uint64_t kInvalidTickCountTime;

 private:
  // InternalSetupRequest and InternalCleanupRequest are helper methods which
  // initialize and cleanup the state of a request.  In addition to managing
  // internal bookkeeping, they also manage a common collection of settings for
  // the various CURL handles used to perform the transfer.  They are called
  // from the CURLTransferEngine constructor and destructor, as well as the
  // ResetForNewRequest method, which is used to reuse a server connection for
  // back-to-back requests.
  void InternalSetupRequest(const std::string& url);
  void InternalCleanupRequest(bool close_handle);

  void CheckForNewUrl();

  // Hooks for CURL's callback model.  The CURL handle is set up so that when
  // there is data to receive or transmit, the static RX or TX function will be
  // called.  The static functions simply use the user data pointer (stream) to
  // call the RX or TX funtion in the appropriate CURLTransferEngine instance.
  static size_t StaticRXFunction(void* ptr,
                                 size_t size,
                                 size_t nmemb,
                                 void* stream);
  static size_t StaticTXFunction(void* ptr,
                                 size_t size,
                                 size_t nmemb,
                                 void* stream);
  static size_t StaticOnHeaderRX(void* ptr,
                                 size_t size,
                                 size_t nmemb,
                                 void* stream);
  static size_t StaticDebugFunction(CURL* ptr,
                                    curl_infotype type,
                                    char *data,
                                    size_t size,
                                    void *stream);


  // The static members used by the static data pump code in order to manage
  // running multiple transfers in parallel.
  static CURLM*           curl_multi_handle_;
  static pthread_mutex_t  multi_handle_lock_;

  static pthread_mutex_t  work_thread_run_lock_;
  static bool             work_thread_should_quit_;
  static pthread_t        work_thread_;
  static PipeEvent        work_thread_wakeup_event_;
  static EngineQueue      finished_transfers_;
  static EngineSet        attn_needed_transfers_;
  static EngineList       timer_needed_transfers_;

  // used by GetTickCount to normalize the timeline.
  static uint64_t         get_tick_count_base_;

  static void  WaitForWork(const uint64_t& timeout);
  static void* WorkThread(void* user_data);
  // GetFDSets will fill out the file descriptor sets which CURL is currently
  // interested in monitoring in order to move data for its various on-going
  // transfers.  It will neither zero out nor reset any of the FDs already
  // flagged by the caller.  Generally, an application should zero out its own
  // FD sets, set any of the file descriptors it is interested in (setting
  // max_fd as it does so) and finally call GetFDSets to pick up the additional
  // FDs that CURL is interested in.
  static bool GetFDSets(fd_set* read_fd_set,
                        fd_set* write_fd_set,
                        fd_set* exc_fd_set,
                        int* max_fd);

  // Pump data is called by the user of the CURLTransferEngine system in order
  // to give the CURL library a chance to move data along.  It will never block
  // on the network, and will return true if the Pump operation caused any
  // transfers to finish (indicating that the application should check its
  // running instances of CURLTransferEngines to find out which one is done and
  // get its results).
  static bool PumpData();

  // The member variables used for any particular transfer operation.  They
  // consist of the CURL handle for the operation itself, the state flags
  // (started, finished, result), the size of chunks to use for TX and RX
  // chains, and the TX and RX chains for the transfer itself.
  CURL* curl_easy_handle_;
  bool transfer_started_;
  bool transfer_finished_;
  CURLcode transfer_result_;
  size_t new_chunk_size_;
  char* user_agent_;
  char* cookies_;
  struct curl_slist* headers_;
  long max_redirects_;
  long connect_timeout_;
  long read_timeout_;
  long ls_bytes_per_sec_;
  long ls_duration_secs_;

  char *current_url_;

  uint64_t start_offset_;
  uint64_t end_offset_;

  uint64_t current_timer_time_;
};

}  // namespace android
#endif  // ANDROID_CURL_TRANSFER_ENGINE_H
