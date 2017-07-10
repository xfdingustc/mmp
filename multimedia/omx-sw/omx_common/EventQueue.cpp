/*
**                Copyright 2014, MARVELL SEMICONDUCTOR, LTD.
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


//#define LOG_NDEBUG 0
#define LOG_TAG "OMXEventQueue"
#include <utils/Log.h>
#include <utils/threads.h>

#include <EventQueue.h>
#include <BerlinOmxCommon.h>

#include <sys/prctl.h>

namespace mmp {

OmxEventQueue::OmxEventQueue(char *threadname)
  : mNextEventID(1),
    mRunning(false),
    mStopped(false) {
  memset(mName, 0x00, 64);
  strcpy(mName, threadname);
  OMX_LOGD("");
}

OmxEventQueue::~OmxEventQueue() {
  stop();
  OMX_LOGD("");
}

void OmxEventQueue::start() {
  OMX_LOGD("");
  if (mRunning) {
    return;
  }

  mStopped = false;

  pthread_create(&mThread, NULL, ThreadWrapper, this);

  mRunning = true;
  OMX_LOGD("");
}

void OmxEventQueue::stop(bool flush) {
  if (!mRunning) {
    return;
  }

  Event *stop = new StopEvent;
  postEvent(stop);

  void *dummy;
  pthread_join(mThread, &dummy);

  for (list<Event*>::iterator it = mQueue.begin(); it != mQueue.end(); ++it) {
    delete (*it);
    it = mQueue.erase(it);
  }
  mQueue.clear();

  mRunning = false;
  OMX_LOGD("");
}

OmxEventQueue::event_id OmxEventQueue::postEvent(Event *event) {
  MmuAutoLock lock(mLock_);

  event->setEventID(mNextEventID++);
  mQueue.push_back(event);
  mQueueNotEmptyCondition_.signal();

  return event->eventID();
}

bool OmxEventQueue::cancelEvent(event_id id) {
  if (id == 0) {
    return false;
  }

  MmuAutoLock lock(mLock_);
  Event *event = removeEventFromQueue_l(id);
  delete event;

  return true;
}

OmxEventQueue::Event* OmxEventQueue::removeEventFromQueue_l(event_id id) {
  for (list<Event*>::iterator it = mQueue.begin(); it != mQueue.end(); ++it) {
    if ((*it)->eventID() == id) {
      Event *event = (*it);
      event->setEventID(0);
      mQueue.erase(it);
      return event;
    }
  }

  OMX_LOGE("Event %d was not found in the queue, already cancelled?", id);
  return NULL;
}

void OmxEventQueue::threadEntry() {
  prctl(PR_SET_NAME, (unsigned long)mName, 0, 0, 0);

  for (;;) {
    Event *event = NULL;
    {
      MmuAutoLock lock(mLock_);

      if (mStopped) {
        OMX_LOGD("");
        break;
      }

      while (mQueue.empty()) {
        mQueueNotEmptyCondition_.wait(mLock_);
      }

      event_id eventID = 0;
      if (mQueue.empty()) {
        // The only event in the queue could have been cancelled
        // while we were waiting for its scheduled time.
        OMX_LOGD("");
        break;
      }
      list<Event*>::iterator it = mQueue.begin();
      eventID = (*it)->eventID();
      event = removeEventFromQueue_l(eventID);
    }

    if (event != NULL) {
      // Fire event with the lock NOT held.
      event->fire(this, 0);
      delete event;
    }
  }
}

// static
void *OmxEventQueue::ThreadWrapper(void *me) {
  static_cast<OmxEventQueue *>(me)->threadEntry();
  return NULL;
}

}
