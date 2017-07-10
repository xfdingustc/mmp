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


#ifndef OMX_EVENT_QUEUE_H_

#define OMX_EVENT_QUEUE_H_

#include <mmu_eventinfo.h>
#include <mmu_thread_utils.h>
#include <BerlinOmxCommon.h>
#include <pthread.h>
#include <list>

using namespace std;

namespace mmp {

class OmxEventQueue {
  public:
    typedef int32_t event_id;

    class Event {
      public:
        Event()
            : mEventID(0) {
        }

        Event(EventInfo &info) : mEventID(0) {
          mEventInfo = info;
        }

        virtual ~Event() {}

        event_id eventID() {
            return mEventID;
        }

      protected:
        virtual void fire(OmxEventQueue *queue, int64_t now_us) = 0;
        EventInfo mEventInfo;

      private:
        friend class OmxEventQueue;

        Event(const Event &);

        Event &operator=(const Event &);

        void setEventID(event_id id) {
            mEventID = id;
        }

        event_id  mEventID;
    };


    OmxEventQueue(char *threadname);
    ~OmxEventQueue();

    // Start executing the event loop.
    void start();

    // Stop executing the event loop, if flush is false, any pending
    // events are discarded, otherwise the queue will stop (and this call
    // return) once all pending events have been handled.
    void stop(bool flush = false);

    // Posts an event to the front of the queue (after all events that
    // have previously been posted to the front but before timed events).
    event_id postEvent(Event *event);

    // Returns true iff event is currently in the queue and has been
    // successfully cancelled. In this case the event will have been
    // removed from the queue and won't fire.
    bool cancelEvent(event_id id);


  private:
    OmxEventQueue(const OmxEventQueue &);
    OmxEventQueue &operator=(const OmxEventQueue &);

    Event* removeEventFromQueue_l(event_id id);

    void threadEntry();
    static void *ThreadWrapper(void *me);

    class StopEvent : public OmxEventQueue::Event {
        virtual void fire(OmxEventQueue *queue, int64_t now_us) {
            queue->mStopped = true;
        }
    };

    char mName[64];
    pthread_t mThread;
    MmuMutex  mLock_;
    MmuCondition mQueueNotEmptyCondition_;

    event_id mNextEventID;
    list<Event*> mQueue;

    bool mRunning;
    bool mStopped;
};

}
#endif
