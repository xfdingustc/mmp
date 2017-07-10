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

extern "C" {
#include "stdlib.h"
}
#include "mmu_mvalue.h"
#include "mmu_macros.h"

namespace mmp {

MValue::MValue()
  : type_(M_TYPE_NONE),
    data_(NULL),
    size_(0) {
}

MValue::~MValue() {
  if (data_) {
    free(data_);
    data_ = NULL;
    size_ = 0;
  }
}

MValue& MValue::operator=(const MValue &val) {
  type_ = val.type_;
  if (data_) {
    free(data_);
    data_ = NULL;
  }
  data_ = malloc(val.size_);
  if (data_) {
    memcpy(data_, val.data_, val.size_);
  }
  size_ = val.size_;

  return *this;
}

void MValue::SetData(MType type, const void* data, muint32 size) {
  type_ = type;
  if (data_) {
    free(data_);
    data_ = NULL;
  }
  data_ = malloc(size);
  memcpy(data_, data, size);
  size_ = size;
}

void MValue::GetData(MType* type, const void** data, muint32* size) {
  *type = type_;
  *size = size_;
  *data = data_;
}

}
