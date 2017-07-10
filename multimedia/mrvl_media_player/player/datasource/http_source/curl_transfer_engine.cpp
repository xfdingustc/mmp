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


// turn on the definitions of INT*_MAX in stdint.  According to the ISO C99
// spec, these definitions are not available in C++ unless specifically
// requested by settting this preprocessor flag.
#define LOG_TAG "HttpDataSource"

#define __STDC_LIMIT_MACROS 1
#include <assert.h>
#include <curl/curl.h>
#include <cutils/properties.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <utils/KeyedVector.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <cutils/properties.h>
#include "http_source/curl_transfer_engine.h"

using std::string;

namespace android {

const int kMinCURLPollPeriod = 200;
const int kVerboseCURLOps = 0;
const long kDefaultMaxRedirects = 5;
const long kDefaultConnectTimeoutInSeconds = 120;
const long kDefaultReadTimeoutInSeconds = 0;
const long kDefaultLSThreshBytesPerSec = 0;
const long kDefaultLSThreshDurationSecs = 0;
const uint64_t CURLTransferEngine::kReadUntilEnd = UINT64_MAX;
const uint64_t CURLTransferEngine::kInvalidTickCountTime = UINT64_MAX;

static void DumpCurlInfo(CURL *ptr, const char *prefix) {
  CURLcode rc;
  const char *url;
  double rtime;
  long rcount; // lint note: long, because CURL uses long.

  rc = curl_easy_getinfo(ptr, CURLINFO_EFFECTIVE_URL, &url);
  if (rc != CURLE_OK)
    url = "dmd-unk";
  rc = curl_easy_getinfo(ptr, CURLINFO_REDIRECT_TIME, &rtime);
  if (rc != CURLE_OK)
    rtime = -500.0;
  rc = curl_easy_getinfo(ptr, CURLINFO_REDIRECT_COUNT, &rcount);
  if (rc != CURLE_OK)
    rcount = -500;

  char enable_pii[PROPERTY_VALUE_MAX];
  property_get("a3ce.log.enable_pii", enable_pii, "0");
  const char* url_to_log = atoi(enable_pii) ? url : "<BLOCKED>";
  ALOGD("%s: Redirect(time=%lf, count=%ld) URL='%s'",
       prefix, rtime, rcount, url_to_log);
}

CURLTransferEngine::CURLTransferEngine(const string& url,
                                       size_t new_chunk_size) {
  user_agent_ = NULL;
  cookies_ = NULL;
  headers_ = NULL;
  current_url_ = NULL;
  curl_easy_handle_ = NULL;
  transfer_started_ = false;
  transfer_finished_ = false;
  transfer_result_ = CURLE_OK;
  new_chunk_size_ = new_chunk_size;
  max_redirects_ = kDefaultMaxRedirects;
  connect_timeout_ = kDefaultConnectTimeoutInSeconds;
  read_timeout_ = kDefaultReadTimeoutInSeconds;
  ls_bytes_per_sec_ = kDefaultLSThreshBytesPerSec;
  ls_duration_secs_ = kDefaultLSThreshDurationSecs;

  start_offset_ = 0;
  end_offset_ = kReadUntilEnd;
  current_timer_time_ = kInvalidTickCountTime;

  InternalSetupRequest(url);
}

CURLTransferEngine::~CURLTransferEngine() {
  // Make sure cleanup has been called
  if (curl_easy_handle_ != NULL) {
      ALOGE("%s - Error! Please call Cleanup on CURLTransferEngine %p "
           "before deleting it", __FUNCTION__, this);
      // Be lenient and call cleanup anyways.
      // We risk corruption since callbacks could be firing
      // while we remove our curl handle, and at this point
      // the object has been partially deleted - A3ceCURLTransferEngine
      // is no more.
      // Hopefully this is a dead code path which is never triggered.
      Cleanup();
  }

  if (user_agent_)
    delete[] user_agent_;

  if (cookies_)
    delete[] cookies_;

  if (headers_)
    curl_slist_free_all(headers_);

  if (current_url_)
    free(current_url_); /* Allocated by strdup! */
}

void CURLTransferEngine::Cleanup() {
  // Grab the work thread lock to prevent callbacks from firing
  // while we remove the multi_handle.
  // Once we return from this function, no callback will be invoke
  // on this engine.
  // NOTE(olivier): grabbing the work thread lock from InternalCleanupRequest
  // results in a deadlock since that function can be called from ::WorkThread,
  // which holds the same lock.
  // We expect the object to be deleted soon after this function returns.
  pthread_mutex_lock(&work_thread_run_lock_);

  // Besides callbacks registered to libcurl directly (which is cleaned up in
  // InternalCleanupRequest), there can be other pending callbacks to the
  // engine from a static curl transfer thread (= ::WorkThread) -- namely,
  // AttentionCallback, TimerCallback, and OnTransferFinished.

  // Cancel any TimerCallback()
  ScheduleTimerCallback(kInvalidTickCountTime, false);

  // Cancel any AttentionCallback()
  attn_needed_transfers_.erase(this);

  // For OnTransferFinished(), finished_transfers_ should be empty
  // because it is filled and emptied without losing work_thread_run_lock_
  // and we are holding this lock here.
  assert(finished_transfers_.empty());

  InternalCleanupRequest(true);
  pthread_mutex_unlock(&work_thread_run_lock_);
}

void CURLTransferEngine::ResetForNewRequest(const string& url) {
  InternalCleanupRequest(false);

  transfer_started_ = false;
  transfer_finished_ = false;
  transfer_result_ = CURLE_OK;
  start_offset_ = 0;
  end_offset_ = kReadUntilEnd;

  InternalSetupRequest(url);
}

bool CURLTransferEngine::SetUserAgent(const char* user_agent) {
  if (transfer_started_) return false;
  if (user_agent_) {
    delete[] user_agent_;
    user_agent_ = NULL;
  }

  if (user_agent) {
    user_agent_ = new char[strlen(user_agent)+1];
    assert(user_agent_);
    if (!user_agent_) return false;
    strcpy(user_agent_, user_agent);
  }
  return true;
}

bool CURLTransferEngine::SetCookies(const char* cookies) {
  if (transfer_started_) return false;
  if (cookies_) {
    delete[] cookies_;
    cookies_ = NULL;
  }

  if (cookies) {
    cookies_ = new char[strlen(cookies)+1];
    assert(cookies_);
    if (!cookies_) return false;
    strcpy(cookies_, cookies);
  }
  return true;
}

bool CURLTransferEngine::SetHeaders(const KeyedVector<String8, String8>* headers) {
  if (headers_) {
    curl_slist_free_all(headers_);
    headers_ = NULL;
  }

  if (NULL != headers) {
    for (size_t i = 0; i < headers->size(); ++i) {
      String8 line;
      line.append(headers->keyAt(i));
      line.append(": ");
      line.append(headers->valueAt(i));
      struct curl_slist *newlist = curl_slist_append(headers_, line.string());
      if (newlist) {
        headers_ = newlist;
      } else {
        curl_slist_free_all(headers_);
        headers_ = NULL;
        return false;
      }
    }
  }
  return true;
}

bool CURLTransferEngine::SetMaxRedirects(long max_redirects) {
  if (transfer_started_) return false;
  max_redirects_ = max_redirects;
  return true;
}

void CURLTransferEngine::InternalSetupRequest(const string& url) {
  // First off, create the easy handle for this transfer.  We may not need to do
  // this since this CURLTransferEngine might be getting reused in order to
  // reuse the TCP connection to the server.
  if (!curl_easy_handle_) {
    curl_easy_handle_ = curl_easy_init();
    assert(curl_easy_handle_);
  }

  // Setup the default options for this transfer.
  if (curl_easy_handle_) {
    CURLcode result;
    // Hook into the Debug function.
    // We use this as a convenient event notification to track URL changes.
    // TODO(dpanpong): Disable after we're done with redirects?
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_DEBUGFUNCTION,
                              StaticDebugFunction);
    assert(CURLE_OK == result);
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_DEBUGDATA,
                              static_cast<void*>(this));
    assert(CURLE_OK == result);
    // Turn on verbose messages so that the debug hook we added above will
    // be called.
    result = curl_easy_setopt(curl_easy_handle_, CURLOPT_VERBOSE, 1L);
    assert(CURLE_OK == result);

    // We never want CURL to attempt to draw any sort of progress bars.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_NOPROGRESS,
                              1L);
    assert(CURLE_OK == result);

    // Configure CURL's connection timeout.
    // This controls the max. time for connection setup.
    if (connect_timeout_) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_CONNECTTIMEOUT,
                                connect_timeout_);
      assert(CURLE_OK == result);
    }

    // Configure CURL's read timeout.
    // This controls the maximum time before we get all of our data from the
    // server (including connection setup, if it's the first read.)
    if (read_timeout_) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_TIMEOUT,
                                read_timeout_);
      assert(CURLE_OK == result);
    }

    // Configure CURL's low speed timeout if configured.
    if ((0 != ls_bytes_per_sec_) && (0 != ls_duration_secs_)) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_LOW_SPEED_LIMIT,
                                ls_bytes_per_sec_);
      assert(CURLE_OK == result);

      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_LOW_SPEED_TIME,
                                ls_duration_secs_);
      assert(CURLE_OK == result);
    }

    // Tell the CURL library to not install any signal handlers.  We really
    // don't want signals used by libCURL to interfere with anything the
    // application may be doing.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_NOSIGNAL,
                              1L);
    assert(CURLE_OK == result);

    // Set the private pointer to 'this'.  When CURL finishes the transfer, we
    // will use this pointer to easily find the CURLTransferEngine associated
    // with this curl_easy_handle.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_PRIVATE,
                              static_cast<void*>(this));
    assert(CURLE_OK == result);

    // Set the URL to use for the transfer.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_URL,
                              url.c_str());
    assert(CURLE_OK == result);

    char proxy_name[PROPERTY_VALUE_MAX];
    char proxy_port[PROPERTY_VALUE_MAX];

    property_get("net.http.proxyHost", proxy_name, "");
    property_get("net.http.proxyPort", proxy_port, "");

    if(strcmp(proxy_name, "")){

        char proxy_dst[PROPERTY_VALUE_MAX] ="";
        strcat(proxy_dst,proxy_name);
        strcat(proxy_dst,":");
        strcat(proxy_dst,proxy_port);

        // Set the http proxy
        result = curl_easy_setopt(curl_easy_handle_, CURLOPT_PROXY, proxy_dst);
        assert(CURLE_OK == result);
    }


    // Set this to our current URL, so that we don't generate a URL change
    // event on the first access.
    if (current_url_)
      free(current_url_);
    current_url_ = strdup(url.c_str());

    // Set the pointer to the callback which CURL should call when it has
    // received data and want to give it to the CURLTransferEngine.  We also set
    // a pointer to this instance as the WRITEDATA; it will be passed to the
    // callback function and will allow us to easily find the appropriate
    // CURLTransferEngine instance.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_WRITEFUNCTION,
                              StaticRXFunction);
    assert(CURLE_OK == result);
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_WRITEDATA,
                              static_cast<void*>(this));
    assert(CURLE_OK == result);

    // Set the pointer to the callback which CURL should call when it has
    // received header data and want to give it to the CURLTransferEngine.  We
    // also set a pointer to this instance as the WRITEHEADER; it will be passed
    // to the callback function and will allow us to easily find the appropriate
    // CURLTransferEngine instance.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_HEADERFUNCTION,
                              StaticOnHeaderRX);
    assert(CURLE_OK == result);
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_WRITEHEADER,
                              static_cast<void*>(this));
    assert(CURLE_OK == result);

    // Until the user of this CURLTransferEngine adds any data to transmit, this
    // will be an HTTP GET operation.
    result = curl_easy_setopt(curl_easy_handle_,
                              CURLOPT_HTTPGET,
                              1L);
    assert(CURLE_OK == result);
  }
}

void CURLTransferEngine::InternalCleanupRequest(bool close_handle) {
  if (curl_easy_handle_) {
    // If this easy handle was started, then it was added to the common multi
    // handle.  Take it off the multi handle (rememember to be in the lock when
    // we do so) and clear the started flag (in case this connection is going to
    // be reused).
    if (transfer_started_) {
      CURLMcode multi_result;
      pthread_mutex_lock(&multi_handle_lock_);

      multi_result = curl_multi_remove_handle(curl_multi_handle_,
                                              curl_easy_handle_);
      assert(CURLM_OK == multi_result);

      pthread_mutex_unlock(&multi_handle_lock_);
      transfer_started_ = false;
    }

    // If we are being called from the destructor, then we will need to close
    // the easy handle as well.
    if (close_handle && (NULL != curl_easy_handle_)) {
      curl_easy_cleanup(curl_easy_handle_);
      curl_easy_handle_ = NULL;
    }
  }
}

bool CURLTransferEngine::BeginTransfer() {
  // Only do something here if the transfer was not alread started.  Users
  // should only call this function once per transfer, but its better to be safe
  // than sorry.
  if (!transfer_started_) {
    CURLMcode multi_result;
    CURLcode result;

    // If the user has data to send to the server with this request, then we
    // need to fill out the appropriate settings on the easy handle to have the
    // data sent to the server.
    if (PostSize()) {
      // Set the read callback.  It will be called when the CURL library wants
      // to read data to transmit.  Its just like the write callback, see
      // InternalSetupRequest for more details.
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_READFUNCTION,
                                StaticTXFunction);
      assert(CURLE_OK == result);
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_READDATA,
                                static_cast<void*>(this));
      assert(CURLE_OK == result);

      // Since we have data to transmit, this is now a POST operation instead of
      // a GET operation.
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_POST,
                                1L);
      assert(CURLE_OK == result);

      // We need to set the POST field size.  This is required by CURL so it can
      // properly fill out headers (like content-length).
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_POSTFIELDSIZE,
                                static_cast<long>(PostSize()));
      assert(CURLE_OK == result);

      // Clear the NOBODY flag (which is on by default).  If we don't do this,
      // CURL will never call us for any data to send.
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_NOBODY,
                                0L);
      assert(CURLE_OK == result);
    }

    // Set the User-Agent string if specified
    if (user_agent_) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_USERAGENT,
                                user_agent_);
      assert(CURLE_OK == result);
    }

    // Set the Cookie string if specified
    if (cookies_) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_COOKIE,
                                cookies_);
      assert(CURLE_OK == result);
    }

    // Set the Headers if specified
    if (headers_) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_HTTPHEADER,
                                headers_);
      assert(CURLE_OK == result);
    }

    // If we are supposed to do anything but fetch the whole file, put the
    // appropriate range header on the request.
    if (start_offset_ || (kReadUntilEnd != end_offset_)) {
      assert(start_offset_ < end_offset_);
      // Format the range of the content to fetch
      char range_buffer[128];
      if (kReadUntilEnd != end_offset_)
        snprintf(range_buffer, sizeof(range_buffer),
                 "%lld-%lld", start_offset_, end_offset_-1);
      else
        snprintf(range_buffer, sizeof(range_buffer), "%lld-", start_offset_);

      // Just in case the snprintf was truncated (should not be possible), make
      // certain it is null terminated.
      range_buffer[sizeof(range_buffer)-1] = '\0';

      // Add the range header.
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_RANGE,
                                range_buffer);
      assert(CURLE_OK == result);
    }

    // Follow redirects up to the limit specified by the user.  If the limit is
    // zero, be sure to disable redirection.
    if (max_redirects_) {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_FOLLOWLOCATION,
                                1L);
      assert(CURLE_OK == result);
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_MAXREDIRS,
                                max_redirects_);
      assert(CURLE_OK == result);
    } else {
      result = curl_easy_setopt(curl_easy_handle_,
                                CURLOPT_FOLLOWLOCATION,
                                0L);
      assert(CURLE_OK == result);
    }

    // Enter the multi lock and add this easy handle to the multi handle.  As
    // soon as the easy handle is attached to the multi handle, CURL is allowed
    // to take action and the transfer is considered started.
    pthread_mutex_lock(&multi_handle_lock_);
    multi_result = curl_multi_add_handle(curl_multi_handle_, curl_easy_handle_);
    pthread_mutex_unlock(&multi_handle_lock_);

    // We should never fail to add this easy handle to the multi handle, but
    // just in case, we do a runtime check and will return false to the caller
    // if something goes wrong.
    assert(CURLM_OK == multi_result);
    transfer_started_ = (CURLM_OK == multi_result);
  }

  // If we started the transfer, wake up the work thread so it can move things
  // along.
  if (transfer_started_)
    work_thread_wakeup_event_.setEvent();

  return transfer_started_;
}

void CURLTransferEngine::InternalWakeupTransfer() {
  // TODO(johngro) : assert that we are holding the work_thread lock.

  // Put this CURLTransferEngine on the list of engines which need attention,
  // then wake up the work thread.  If we are already on the list of engines
  // which need attn, the insert will simply fail.
  attn_needed_transfers_.insert(this);
  work_thread_wakeup_event_.setEvent();;
}

void CURLTransferEngine::WakeupTransfer() {
  pthread_mutex_lock(&work_thread_run_lock_);
  InternalWakeupTransfer();
  pthread_mutex_unlock(&work_thread_run_lock_);
}

void CURLTransferEngine::ScheduleTimerCallback(uint64_t wakeup_time,
                                               bool lock_work_thread) {
  // Nothing to do if our current timeout and the scheduled timeout are the
  // same.
  if (wakeup_time == current_timer_time_)
    return;

  if (lock_work_thread)
    pthread_mutex_lock(&work_thread_run_lock_);

  // If we are already scheduled, we need to remove our previous entry from the
  // list.  It is understood that this is an order N operation, but since N is
  // never going to greater than 3-4 (and most of the time it will be 1), it
  // shouldn't really matter.
  if (kInvalidTickCountTime != current_timer_time_) {
    EngineList::iterator remove_me = std::find(timer_needed_transfers_.begin(),
                                               timer_needed_transfers_.end(),
                                               this);
    assert(timer_needed_transfers_.end() != remove_me);
    timer_needed_transfers_.erase(remove_me);
  }

  // Now, if a timer callback was requested, find the place in the timer list
  // that this engine belongs and insert it there.
  if (kInvalidTickCountTime != (current_timer_time_ = wakeup_time)) {
    EngineList::iterator iter;

    // For loop to find the proper position in the list to insert the element.
    // Note, the body of the loop is empty on purpose.
    for (iter = timer_needed_transfers_.begin();
         (iter != timer_needed_transfers_.end()) &&
         ((*iter)->current_timer_time_ < wakeup_time);
         ++iter);

    timer_needed_transfers_.insert(iter, this);
  }

  // Finally, wakeup the work thread to let it know that there has been a change
  // to the timer callback schedule.  Note, we don't wakeupt the thread if we
  // just canceled a timer.  The work thread will find out that the timer was
  // canceled automatically the next time it wakes up.
  if (kInvalidTickCountTime != current_timer_time_)
    work_thread_wakeup_event_.setEvent();

  if (lock_work_thread)
    pthread_mutex_unlock(&work_thread_run_lock_);
}

bool CURLTransferEngine::SetRXRange(uint64_t start_loc, uint64_t end_loc) {
  if (transfer_started_)
    return false;

  if (start_loc >= end_loc)
    return false;

  start_offset_ = start_loc;
  end_offset_ = end_loc;
  return true;
}

void CURLTransferEngine::CheckForNewUrl() {
  char *url;
  CURLcode rc;

  rc = curl_easy_getinfo(curl_easy_handle_, CURLINFO_EFFECTIVE_URL, &url);
  if (rc != CURLE_OK || !url) /* No URL.*/
    return;

  if (current_url_ && strcmp(current_url_, url)) {
    free(current_url_);
    current_url_ = NULL;
  }
  if (!current_url_) {
    current_url_ = strdup(url);
    if (current_url_) {
      OnURLChange(current_url_);
    } else {
      ALOGW("Out of memory copying new URL.");
    }
  }
}

size_t CURLTransferEngine::StaticRXFunction(void* ptr,
                                            size_t size,
                                            size_t nmemb,
                                            void* stream) {
  if (!stream) return 0;

  CURLTransferEngine* engine = static_cast<CURLTransferEngine*>(stream);

  return engine->RXFunction(ptr, size, nmemb);
}

size_t CURLTransferEngine::StaticTXFunction(void* ptr,
                                            size_t size,
                                            size_t nmemb,
                                            void* stream) {
  if (!stream) return 0;

  CURLTransferEngine* engine = static_cast<CURLTransferEngine*>(stream);

  return engine->TXFunction(ptr, size, nmemb);
}

size_t CURLTransferEngine::StaticOnHeaderRX(void* ptr,
                                            size_t size,
                                            size_t nmemb,
                                            void* stream) {
  if (!stream) return -1;
  return (static_cast<CURLTransferEngine*>(stream))->OnHeaderRX(ptr,
                                                                size,
                                                                nmemb);
}

// This function is called by CURL's "debug" hook.
// Every time it is called, it tests to see whether the URL has changed.
// In addition, it logs the messages that CURL provides, for additional
// debug.
size_t CURLTransferEngine::StaticDebugFunction(CURL* ptr,
                                               curl_infotype type,
                                               char *data,
                                               size_t size,
                                               void *stream) {
  if (!stream) return -1;
  if (kVerboseCURLOps && type == CURLINFO_TEXT) {
    DumpCurlInfo(ptr, "Debug");
    /* Per CURL documentation, these messages are not zero-terminated. */
    if (size > 0) {
      char msg[256];
      if (size > sizeof(msg)) size = sizeof(msg);
      strncpy(msg, data, size);
      msg[size-1] = '\0';
      ALOGD("%s", msg);
    }
  }

  CURLTransferEngine* engine = static_cast<CURLTransferEngine*>(stream);
  engine->CheckForNewUrl();
  return 0;
}

size_t CURLTransferEngine::RXFunction(void* ptr, size_t size, size_t nmemb) {
  // Default implementation, lie and say we consumed all of the data.  Most
  // applications will override this method and do something meaningful with the
  // data.
  return (size * nmemb);
}

size_t CURLTransferEngine::TXFunction(void* ptr, size_t size, size_t nmemb) {
  // If no one overrides this method, then we have nothing to send.  Just return
  // 0 to indicate that we have no body.  Normally, this should never happen.
  // Clients who wish to send a body along with their request will also override
  // PostSize to indicate the non-zero size of the data they have to post.
  return 0;
}

size_t CURLTransferEngine::OnHeaderRX(void* ptr, size_t size, size_t nmemb) {
  // Default implementation, lie and say we consumed all of the data.  Most
  // applications will override this method and do something meaningful with the
  // data.
  return (size * nmemb);
}

void CURLTransferEngine::OnURLChange(const char *url) {
  // Do nothing by default.
}

void CURLTransferEngine::OnTransferFinished() {
  // Do nothing by default.
}

size_t CURLTransferEngine::PostSize() {
  // Default implementation is a simple GET operation.
  return 0;
}

void CURLTransferEngine::AttentionCallback() {
  // Default implementation of the AttentionCallback does nothing.
}

void CURLTransferEngine::TimerCallback() {
  // Default implementation of the TimerCallback does nothing.
}

}  // namespace android

