/*
 **                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
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

#ifndef MMP_BUFFERLIST_H
#define MMP_BUFFERLIST_H



#include "mmu_thread.h"
#include <utils/Log.h>

using namespace mmu;

namespace mmp {
 // On create, queue size is 5M, when queue is full, increase 1M until it reaches 10M.
#define DEFAULT_MAX_SIZE (1024 * 1024 * 5)
#define MAX_QUEUE_SIZE   (1024 * 1024 * 10)
#define INCREASE_STEP    (1024 * 1024)

class MmpBufferList {
  public:
   explicit MmpBufferList(uint32_t max_byte = DEFAULT_MAX_SIZE);
   ~MmpBufferList();

   bool       pushBack(MmpBuffer* mmp_buf);
   MmpBuffer* popFront();
   MmpBuffer* begin(); // Get the first item in list, but don't remove it from list.
   //ESPacket* getAt(int32_t pos);
   MmpBuffer* getNext();
   bool       flush();
   bool      resetCurrentPos();
   uint32_t  getFirstPacketSize();
   uint32_t  getCapacity() { return max_bytes_; }
   uint32_t  getBufferedSize() { return queued_bytes_; }
   bool      increaseBuffer();
   bool      canQueueMore(uint32_t advance);

  private:
   typedef struct _PacketBufferList {
     MmpBuffer* buf;
     struct _PacketBufferList *next;
   } PacketBufferList;

   MmuMutex queue_lock_;
   PacketBufferList *data_list_;

   // How many bytes have been queued.
   uint32_t queued_bytes_;
   uint32_t max_bytes_;

   // How many packets have been queued.
   int packets_;

   // The current position in the list
   uint32_t current_pos_;
   uint32_t list_size_;
};

}
#endif
