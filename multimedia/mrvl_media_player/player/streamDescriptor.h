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

#ifndef MRVL_STREAM_DESCRIPTOR_H
#define MRVL_STREAM_DESCRIPTOR_H

extern "C" {
#include "stdio.h"
}

#include "frameworks/mmp_frameworks.h"

namespace mmp {

struct streamDescriptor {
  streamDescriptor(const mchar *mime, const char *lan, const char *des)
    : mime_type(NULL),
      language(NULL),
      description(NULL) {

    if (mime) {
      mime_type = new char[strlen(mime) + 1];
      if (mime_type) {
        memset(mime_type, 0x00, strlen(mime) + 1);
        strcpy(mime_type, mime);
      }
    }

    if (lan) {
      language = new char[strlen(lan) + 1];
      if (language) {
        memset(language, 0x00, strlen(lan) + 1);
        strcpy(language, lan);
      }
    }

    if (des) {
      description = new char[strlen(des) + 1];
      if (description) {
        memset(description, 0x00, strlen(des) + 1);
        strcpy(description, des);
      }
    }
  }

  ~streamDescriptor() {
    delete [] mime_type;
    delete [] language;
    delete [] description;
  }

  char *mime_type;
  char *language;
  char *description;
};

struct streamDescriptorList{
  streamDescriptorList() {}

  ~streamDescriptorList() {
    while (!descriptors.empty()) {
      streamDescriptor *desc = *(descriptors.begin());
      delete desc;
      descriptors.erase(descriptors.begin());
    }
  }

  void pushItem(streamDescriptor *sd) {
    if (sd) {
      descriptors.push_back(sd);
    }
  }

  vector<streamDescriptor *> descriptors;
};

}
#endif
