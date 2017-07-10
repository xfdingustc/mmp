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

#include "mmp_buffer.h"
#include "mmp_bufferlist.h"

#define LOG_TAG "ESPacketList"


namespace mmp {
MmpBufferList::MmpBufferList(uint32_t max_byte)
  : data_list_(NULL),
    queued_bytes_(0),
    packets_(0),
    current_pos_(0),
    list_size_ (0) {
  max_bytes_ = max_byte;
}

MmpBufferList::~MmpBufferList() {
  flush();
}

bool MmpBufferList::pushBack(MmpBuffer* mmp_buf) {
  PacketBufferList *new_node = NULL;
  PacketBufferList *tmp_node = NULL;

  if (!mmp_buf) {
    return false;
  }

  MmuAutoLock lock(queue_lock_);

  if (!data_list_) {
    // Add the first node.
    data_list_ = (PacketBufferList *)malloc(sizeof(PacketBufferList));
    if (NULL == data_list_) {
      ALOGE("%s() line %d failed to malloc a PacketBufferList", __FUNCTION__, __LINE__);
      return false;
    }
    data_list_->buf = mmp_buf;
    data_list_->next = NULL;
  } else {
    // Create a new node.
    new_node = (PacketBufferList *)malloc(sizeof(PacketBufferList));
    if (NULL == new_node) {
      ALOGE("%s() line %d failed to malloc a PacketBufferList", __FUNCTION__, __LINE__);
      return false;
    }
    new_node->buf = mmp_buf;
    new_node->next = NULL;

    // Find the last node in data_list_ and append new node.
    tmp_node = data_list_;
    while (tmp_node && tmp_node->next) {
      tmp_node = tmp_node->next;
    }
    tmp_node->next = new_node;
  }

  if (mmp_buf) {
    queued_bytes_ += mmp_buf->size;
  }

  list_size_++;

  return true;
}

MmpBuffer* MmpBufferList::popFront() {
  PacketBufferList *node = NULL;
  MmpBuffer* pkt = NULL;

  MmuAutoLock lock(queue_lock_);
  if (!data_list_) {
    return NULL;
  }
  if (data_list_) {
    node = data_list_;
    data_list_ = data_list_->next;

    pkt = node->buf;
    free(node);
    node = NULL;
  }

  if (pkt) {
    queued_bytes_ -= pkt->size;
  }

  list_size_--;

  return pkt;
}

// Get the first item in list, but don't remove it from list.
MmpBuffer* MmpBufferList::begin() {
  PacketBufferList *node = NULL;
  MmpBuffer *pkt = NULL;

  MmuAutoLock lock(queue_lock_);
  if (!data_list_) {
    return NULL;
  }
  if (data_list_) {
    node = data_list_;
    pkt = node->buf;
  }

  return pkt;
}

MmpBuffer* MmpBufferList::getNext() {
  PacketBufferList *node = NULL;

  MmuAutoLock lock(queue_lock_);
  if (!data_list_) {
    return NULL;
  }

  if (current_pos_ >= list_size_) {
    return NULL;
  }

  node = data_list_;

  for (uint32_t i = 0; i < current_pos_; i++) {
    node = node->next;
    if (!node) {
      return NULL;
    }
  }

  current_pos_++;
  return node->buf;
}


bool MmpBufferList::resetCurrentPos() {
  MmuAutoLock lock(queue_lock_);
  current_pos_ = 0;
  return true;
}

bool MmpBufferList::flush() {
  PacketBufferList *node = NULL;

  MmuAutoLock lock(queue_lock_);
  current_pos_ = 0;
  while (data_list_) {
    node = data_list_;
    data_list_ = data_list_->next;

    if (node->buf) {
      delete node->buf;
      node->buf = NULL;
    }
    free(node);
    node = NULL;
  }
  data_list_ = NULL;
  queued_bytes_ = 0;
  // After flush(), reset default max queue size.
  max_bytes_ = DEFAULT_MAX_SIZE;

  return true;
}

uint32_t MmpBufferList::getFirstPacketSize() {
  MmuAutoLock lock(queue_lock_);
  if (data_list_ && data_list_->buf) {
    return data_list_->buf->size;
  }
  return 0;
}

bool MmpBufferList::increaseBuffer() {
  MmuAutoLock lock(queue_lock_);
  if (max_bytes_ < MAX_QUEUE_SIZE) {
    max_bytes_ += INCREASE_STEP;
  }
  return true;
}

bool MmpBufferList::canQueueMore(uint32_t advance) {
  return (queued_bytes_ + advance) <= max_bytes_;
}

}
