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

#include <stdarg.h>

#include "mmu_eventinfo.h"
#include "mmu_macros.h"

namespace mmp {

EventInfo::EventInfo(const mchar* media_type, const mchar* field_name, ...) {

}

EventInfo::~EventInfo() {
  cleanItems();
}

EventInfo& EventInfo::operator=(const EventInfo &caps) {
  // Firstly clean items_;
  cleanItems();

  // Secondly copy caps.
  EventInfo& newcaps = const_cast<EventInfo &>(caps);
  map<MmuHash, MValue*>::iterator it;
  for (it = newcaps.items_.begin(); it != newcaps.items_.end(); it++) {
    MValue* new_value = new MValue;
    if (!new_value) {
      break;
    }
    // Call operator= to copy vaule.
    *new_value = *(it->second);
    items_.insert(pair<MmuHash, MValue*>(it->first, new_value));
  }

  return *this;
}

mbool EventInfo::cleanItems() {
  map<MmuHash, MValue*>::iterator it;
  for (it = items_.begin(); it != items_.end(); it++) {
    delete it->second;
  }
  items_.clear();

  return true;
}

void EventInfo::SetString(const mchar* field, const mchar* value) {
  MmuHash field_hash = GetHashFromString(field);

  MValue* new_value = new MValue;

  new_value->SetData(M_TYPE_CHAR, value, strlen(value) + 1);

  items_.insert(pair<MmuHash, MValue*>(field_hash, new_value));

}

void EventInfo::SetInt32(const mchar* field, const mint32 value) {
  MmuHash field_hash = GetHashFromString(field);

  MValue* new_value = new MValue;
  new_value->SetData(M_TYPE_INT32, &value, sizeof(value));

  items_.insert(pair<MmuHash, MValue*>(field_hash, new_value));

}

void EventInfo::SetData(const mchar* field, const void* value, mint32 size) {
  MmuHash field_hash = GetHashFromString(field);

  MValue* new_value = new MValue;
  new_value->SetData(M_TYPE_PTR, value, size);

  items_.insert(pair<MmuHash, MValue*>(field_hash, new_value));
}

void EventInfo::SetPointer(const mchar* field, void *value) {
  MmuHash field_hash = GetHashFromString(field);

  MValue* new_value = new MValue;
  new_value->SetData(M_TYPE_PTR, &value, sizeof(value));

  items_.insert(pair<MmuHash, MValue*>(field_hash, new_value));
}

mbool EventInfo::GetString(const mchar* field, const mchar** value) {
  MmuHash field_hash = GetHashFromString(field);

  map<MmuHash, MValue*>::iterator it;

  it = items_.find(field_hash);
  M_ASSERT_FATAL((it != items_.end()), false);

  MValue* found_value = it->second;

  muint32 size = 0;
  const void* data;
  MType type;

  found_value->GetData(&type, &data, &size);
  M_ASSERT_FATAL((type == M_TYPE_CHAR), false);

  *value = static_cast<const mchar*>(data);

  return true;

}

mbool EventInfo::GetInt32(const mchar* field, mint32* value) {
  MmuHash field_hash = GetHashFromString(field);

  map<MmuHash, MValue*>::iterator it;

  it = items_.find(field_hash);
  M_ASSERT_FATAL((it != items_.end()), false);

  MValue* found_value = it->second;

  muint32 size = 0;
  const void* data;
  MType type;

  found_value->GetData(&type, &data, &size);
  M_ASSERT_FATAL((type == M_TYPE_INT32), false);

  *value = *((mint32*)(data));

  return true;
}

mbool EventInfo::GetData(const mchar* field, const void** value, mint32* size) {
  MmuHash field_hash = GetHashFromString(field);

  map<MmuHash, MValue*>::iterator it;

  it = items_.find(field_hash);
  M_ASSERT_FATAL((it != items_.end()), false);

  MValue* found_value = it->second;

  muint32 datasize = 0;
  const void* data;
  MType type;

  found_value->GetData(&type, &data, &datasize);
  M_ASSERT_FATAL((type == M_TYPE_PTR), false);

  *value = data;
  *size = datasize;

  return true;
}

mbool EventInfo::GetPointer(const mchar* field, void **value) {
  MmuHash field_hash = GetHashFromString(field);

  map<MmuHash, MValue*>::iterator it;

  it = items_.find(field_hash);
  M_ASSERT_FATAL((it != items_.end()), false);

  MValue* found_value = it->second;

  muint32 datasize = 0;
  const void* data;
  MType type;

  found_value->GetData(&type, &data, &datasize);
  M_ASSERT_FATAL((type == M_TYPE_PTR), false);
  M_ASSERT_FATAL(datasize, sizeof(*value));

  *value = *(void **)data;

  return true;
}

}

