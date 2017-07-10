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

#define LOG_TAG "PipeEvent"
#include <utils/Log.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "thread_utils.h"

namespace android {

PipeEvent::PipeEvent() {
  pipe_[0] = -1;
  pipe_[1] = -1;

  // Create the pipe.
  if (pipe(pipe_) >= 0) {
    // Set non-blocking mode on the read side of the pipe so we can easily
    // drain it whenever we wakeup.
    fcntl(pipe_[0], F_SETFL, O_NONBLOCK);
  } else {
    ALOGE("Failed to create pipe event %d %d %d",
              pipe_[0], pipe_[1], errno);
    pipe_[0] = -1;
    pipe_[1] = -1;
  }
}

PipeEvent::~PipeEvent() {
  if (pipe_[0] >= 0)
    close(pipe_[0]);

  if (pipe_[1] >= 0)
    close(pipe_[1]);
}

void PipeEvent::clearPendingEvents() {
  char drain_buffer[256];
  while (read(pipe_[0], drain_buffer, sizeof(drain_buffer)) > 0) {
    // No body.
  }
}

bool PipeEvent::wait(int timeout) {
  struct pollfd wait_fd;

  wait_fd.fd = getWakeupHandle();
  wait_fd.events = POLLIN;
  wait_fd.revents = 0;

  int res = poll(&wait_fd, 1, timeout);

  if (res < 0) {
    ALOGE("Wait error in PipeEvent; sleeping to prevent overload!");
    usleep(1000);
  }

  return (res > 0);
}

void PipeEvent::setEvent() {
  char foo = 'q';
  write(pipe_[1], &foo, 1);
}

}  // namespace android