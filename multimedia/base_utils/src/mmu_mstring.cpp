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

#include "mmu_mstring.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


namespace mmu {

static const char* EMPTY_STRING = "";

MString::MString()
  : data_((mchar *)EMPTY_STRING),
    size_(0),
    alloc_size_(1) {
}

MString::MString(const mchar *s)
  : data_(NULL),
    size_(0),
    alloc_size_(1) {
  SetTo(s);
}

MString::MString(const mchar *s, muint32 size)
  : data_(NULL),
    size_(0),
    alloc_size_(1) {
  SetTo(s, size);
}


MString::~MString() {
  Clear();
}

void MString::SetTo(const mchar *s) {
  SetTo(s, strlen(s));
}

void MString::SetTo(const mchar *s, muint32 size) {
  Clear();
  Append(s, size);
}

void MString::Clear() {
  if (data_ && data_ != EMPTY_STRING) {
    free(data_);
    data_ = NULL;
  }

  data_ = (mchar *)EMPTY_STRING;
  size_ = 0;
  alloc_size_ = 1;
}

void MString::Trim() {
  makeMutable();

  muint32 i = 0;
  while (i < size_ && isspace(data_[i])) {
    ++i;
  }

  muint32 j = size_;
  while (j > i && isspace(data_[j - 1])) {
    --j;
  }

  memmove(data_, &data_[i], j - i);
  size_ = j - i;
  data_[size_] = '\0';
}


void MString::Erase(muint32 start, muint32 n) {
  if (start >= size_) {
    return;
  }
  if (start + n > size_) {
    return;
  }

  makeMutable();

  memmove(&data_[start], &data_[start + n], size_ - start - n);
  size_ -= n;
  data_[size_] = '\0';
}

void MString::Append(const char *s) {
  Append(s, strlen(s));
}

void MString::Append(const char *s, muint32 size) {
  makeMutable();

  if (size_ + size + 1 > alloc_size_) {
    alloc_size_ = (alloc_size_ + size + 31) & -32;
    data_ = (char *)realloc(data_, alloc_size_);
    if(data_ == NULL) {
      return;
    }
  }

  memcpy(&data_[size_], s, size);
  size_ += size;
  data_[size_] = '\0';
}


void MString::makeMutable() {
  if (data_ == EMPTY_STRING) {
    data_ = strdup(EMPTY_STRING);
  }
}


}
