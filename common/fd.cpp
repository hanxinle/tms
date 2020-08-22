#include "fd.h"

#include <poll.h>
#include <unistd.h>

#include "common_define.h"
#include "io_loop.h"

std::atomic<uint64_t> Fd::id_generator_;

Fd::Fd(IoLoop* io_loop, const int& fd)
    : events_(0),
      fd_(fd),
      io_loop_(io_loop),
      socket_handler_(NULL),
      id_(GenID()),
      name_("unknown") {}

Fd::~Fd() {
  if (fd_ > 0) {
    DisableRead();
    DisableWrite();

    close(fd_);
  }
}

void Fd::EnableRead() {
  if (events_ & POLLIN) {
    return;
  }

  bool add = true;

  if (events_ != 0) {
    add = false;
  }

  events_ |= POLLIN;

  if (add) {
    io_loop_->AddFd(this);
  } else {
    io_loop_->ModFd(this);
  }
}

void Fd::EnableWrite() {
  if (events_ & POLLOUT) {
    return;
  }

  bool add = true;

  if (events_ != 0) {
    add = false;
  }

  events_ |= POLLOUT;

  if (add) {
    io_loop_->AddFd(this);
  } else {
    io_loop_->ModFd(this);
  }
}

void Fd::DisableRead() {
  if ((events_ & POLLIN) == 0) {
    return;
  }

  bool del = false;

  events_ &= (~POLLIN);
  if (events_ == 0) {
    del = true;
  }

  if (del) {
    io_loop_->DelFd(this);
  } else {
    io_loop_->ModFd(this);
  }
}

void Fd::DisableWrite() {
  if ((events_ & POLLOUT) == 0) {
    return;
  }

  bool del = false;

  events_ &= (~POLLOUT);
  if (events_ == 0) {
    del = true;
  }

  if (del) {
    io_loop_->DelFd(this);
  } else {
    io_loop_->ModFd(this);
  }
}
