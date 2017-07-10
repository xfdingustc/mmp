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


#include <assert.h>
#include <curl/curl.h>
#include <sys/prctl.h>

#include "http_source/curl_transfer_engine.h"
#include "thread_utils.h"

namespace android {

CURLM*          CURLTransferEngine::curl_multi_handle_ = NULL;
pthread_mutex_t CURLTransferEngine::multi_handle_lock_ =
    PTHREAD_MUTEX_INITIALIZER;
pthread_t       CURLTransferEngine::work_thread_ = 0;
pthread_mutex_t CURLTransferEngine::work_thread_run_lock_ =
    PTHREAD_MUTEX_INITIALIZER;
bool            CURLTransferEngine::work_thread_should_quit_ = false;
PipeEvent       CURLTransferEngine::work_thread_wakeup_event_;
EngineQueue     CURLTransferEngine::finished_transfers_;
EngineSet       CURLTransferEngine::attn_needed_transfers_;
EngineList      CURLTransferEngine::timer_needed_transfers_;
uint64_t        CURLTransferEngine::get_tick_count_base_ = 0;

bool CURLTransferEngine::Initialize() {
  bool ret_val = false;
  pthread_mutex_lock(&multi_handle_lock_);

  if (!get_tick_count_base_)
    get_tick_count_base_ = GetTickCount();

  if (!curl_multi_handle_)
    curl_multi_handle_ = curl_multi_init();

  if (curl_multi_handle_) {
    if (!work_thread_) {
      if (pthread_create(&work_thread_,
                         NULL,
                         CURLTransferEngine::WorkThread,
                         NULL)) {
        // TODO(johngro) : log an error indicating that the thread didn't start.
      } else
        ret_val = true;
    } else
      ret_val = true;
  }

  pthread_mutex_unlock(&multi_handle_lock_);

  if (!ret_val)
    Shutdown();

  return ret_val;
}

void CURLTransferEngine::Shutdown() {
  pthread_mutex_lock(&multi_handle_lock_);

  if (work_thread_) {
    work_thread_should_quit_ = true;
    work_thread_wakeup_event_.setEvent();

    void* thread_ret;
    pthread_join(work_thread_, &thread_ret);
    work_thread_ = 0;

    work_thread_should_quit_ = false;
  }

  if (curl_multi_handle_) {
    CURLMcode multi_result;
    multi_result = curl_multi_cleanup(curl_multi_handle_);
    assert(CURLM_OK == multi_result);
    curl_multi_handle_ = NULL;
  }

  while (finished_transfers_.size())
    finished_transfers_.pop();

  pthread_mutex_unlock(&multi_handle_lock_);
}

bool CURLTransferEngine::GetFDSets(fd_set* read_fd_set,
                                   fd_set* write_fd_set,
                                   fd_set* exc_fd_set,
                                   int* max_fd) {
  // If we are not initialized, don't do anything.
  if (!curl_multi_handle_)
    return false;

  if (!read_fd_set || !write_fd_set || !exc_fd_set || !max_fd)
    return false;

  // Enter the multi handle lock.  Any time we call a CURL function on the
  // common multi handle, we need to be in this lock.
  pthread_mutex_lock(&multi_handle_lock_);

  // Fetch from CURL the file descriptors it is interested in.
  int curl_max_fd;
  CURLMcode multi_result;
  multi_result = curl_multi_fdset(curl_multi_handle_,
                                  read_fd_set,
                                  write_fd_set,
                                  exc_fd_set,
                                  &curl_max_fd);
  assert(CURLM_OK == multi_result);

  // If the max FD CURL is interested in is greater than the max FD the caller
  // is interested in, go ahead and update it.
  if (curl_max_fd > *max_fd)
    *max_fd = curl_max_fd;

  pthread_mutex_unlock(&multi_handle_lock_);

  return true;
}

bool CURLTransferEngine::PumpData() {
  bool ret_val = false;
  int running_handles;

  // If we are not initialized, don't do anything.
  if (!curl_multi_handle_)
    return false;

  // Enter the multi_handle_lock.  We cannot have other threads adding or
  // removing easy handles to the multi handle while we are pumping the data.
  pthread_mutex_lock(&multi_handle_lock_);

  // Call curl_multi_perform at least once, and keep calling it as long as CURL
  // indicates that there is more work to do.  CURL will never block on the
  // network during this call, so this should always run fairly quickly.
  CURLMcode pump_result;
  do {
    pump_result = curl_multi_perform(curl_multi_handle_, &running_handles);
  } while (CURLM_CALL_MULTI_PERFORM == pump_result);
  assert(CURLM_OK == pump_result);

  // Now check to see if the data pumping has generated any messages from the
  // CURL library on the curl_multi_handle.  In particular, we need to check to
  // see if any of the easy_handles have finished their transfers.  For now, we
  // ignore any other messages.
  CURLMsg* done_msg;
  int msgs_in_queue;
  while ((done_msg = curl_multi_info_read(curl_multi_handle_,
                                          &msgs_in_queue))) {
    if (done_msg && (done_msg->msg == CURLMSG_DONE)) {
      // At least one job has completed.  We return true to our caller to
      // indicate that he/she should check their ongoing transfers to collect
      // their results from the finished transfers.
      ret_val = true;

      // Get the user data pointer (the CURLINFO_PRIVATE setting) from the CURL
      // easy handle.  It points to the CURLTransferEngine which owns the easy
      // handle.  Then mark that the transfer has finished and cache the
      // transfer result which came back from the CURL library.
      void* obj_handle;
      CURLcode tmp;
      tmp = curl_easy_getinfo(done_msg->easy_handle,
                              CURLINFO_PRIVATE,
                              &obj_handle);
      assert(CURLE_OK == tmp);

      if ((CURLE_OK == tmp) && obj_handle) {
        CURLTransferEngine* transfer;
        transfer = static_cast<CURLTransferEngine*>(obj_handle);
        assert(transfer);
        if (transfer) {
          transfer->transfer_finished_ = true;
          transfer->transfer_result_ = done_msg->data.result;
          finished_transfers_.push(transfer);
        }
      }
    }
  }

  pthread_mutex_unlock(&multi_handle_lock_);

  return ret_val;
}

// Called by the work thread to block until there is something to do.  The
// implementation does a select on all the currently active socket FDs in use
// by CURL at the moment as well as a pipe created by the library which is used
// to wake up the work thread when something happens which needs attention
// (like a change in logging endpoint, or a command to shutdown.)
void CURLTransferEngine::WaitForWork(const uint64_t& timeout) {
  assert(work_thread_wakeup_event_.successfulInit());

  fd_set rd_set;
  fd_set wr_set;
  fd_set ex_set;
  int    max_fd = work_thread_wakeup_event_.getWakeupHandle();

  FD_ZERO(&rd_set);
  FD_ZERO(&wr_set);
  FD_ZERO(&ex_set);

  // Gather together the file descriptors we are going to wait on.  We
  // always wait on the read half of our wakeup pipe; we also need to
  // wait on and of the FDs being used by the CURLTransferEngine.
  assert((max_fd > 0) && (max_fd < FD_SETSIZE));
  FD_SET(max_fd, &rd_set);
  CURLTransferEngine::GetFDSets(&rd_set, &wr_set, &ex_set, &max_fd);

  // CURL recomends (as a matter of best practice) driving its data pump at
  // least once every couple of seconds when there is an ongoing transfer to
  // ensure proper functioning of its timeout behavior.  Right now, the
  // kMinCURLPollPeriod variable controlls this behavior.
  uint64_t usec_timeout = kMinCURLPollPeriod;
  if ((kInvalidTickCountTime != timeout) &&
      (timeout < usec_timeout))
    usec_timeout = timeout;

  struct timeval next_timeout;
  next_timeout.tv_usec = ((usec_timeout % 1000) * 1000);
  next_timeout.tv_sec  =  (usec_timeout / 1000);

  // Go ahead and do the select.
  select(max_fd+1, &rd_set, &wr_set, &ex_set, &next_timeout);

  // Now that we have woken up, drain any of the 'wakeup bytes' which
  // have been crammed into the wakeup pipe.  The pipe has been set to
  // non-blocking, so once it is drained, we won't end up getting stuck.
  work_thread_wakeup_event_.clearPendingEvents();
}

void* CURLTransferEngine::WorkThread(void* user_data) {
  // Name our thread to assist in debugging.
  prctl(PR_SET_NAME, "CurlHttpWk");

  // Grab the run lock and enter the main work loop.
  pthread_mutex_lock(&work_thread_run_lock_);

  while (!work_thread_should_quit_) {
    // Process the list of transfers which need attention.
    for (EngineSet::iterator it = attn_needed_transfers_.begin();
         it != attn_needed_transfers_.end();
         ++it) {
      CURLTransferEngine* engine = *it;
      assert(engine);
      engine->AttentionCallback();
    }
    attn_needed_transfers_.clear();

    // Keep pumping data until there is nothing left to do.
    if (PumpData())
      continue;

    while (finished_transfers_.size()) {
      CURLTransferEngine* finished = finished_transfers_.front();
      finished_transfers_.pop();
      assert(finished);

      finished->OnTransferFinished();
    }

    // Process the list of transfers whose timers have fired.
    uint64_t now_time = GetTickCount();
    while (timer_needed_transfers_.size() &&
          (timer_needed_transfers_.front()->current_timer_time_ <= now_time)) {
      // Take our target engine off the list and mark it as not having a
      // scheduled timer.
      CURLTransferEngine* engine = timer_needed_transfers_.front();
      timer_needed_transfers_.pop_front();
      engine->current_timer_time_ = kInvalidTickCountTime;
      engine->TimerCallback();
    }

    uint64_t timeout_amt = kInvalidTickCountTime;
    if (timer_needed_transfers_.size())
      timeout_amt = now_time -
                    timer_needed_transfers_.front()->current_timer_time_;

    // Time to wait for something to do.  Leave the run lock (since we are
    // no longer running) and go so sleep on the wakeup event.  As soon as
    // we wake up, re-enter the run lock and go around again.
    pthread_mutex_unlock(&work_thread_run_lock_);
    WaitForWork(timeout_amt);
    pthread_mutex_lock(&work_thread_run_lock_);
  }
  pthread_mutex_unlock(&work_thread_run_lock_);
  return NULL;
}

}  // namespace android