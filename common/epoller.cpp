#include "epoller.h"

#include "common_define.h"
#include "fd.h"
#include "util.h"

#if defined(__APPLE__)
#include <poll.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#else
#include <sys/epoll.h>
#endif
#include <assert.h>
#include <unistd.h>

#include <iostream>

Epoller::Epoller() : IoLoop() {}

Epoller::~Epoller() {
  if (poll_fd_ > 0) {
    close(poll_fd_);
  }
}

int Epoller::Create() {
#if defined(__APPLE__)
  if (poll_fd_ < 0) {
    poll_fd_ = kqueue();

    if (poll_fd_ < 0) {
      std::cout << LMSG << "kqueue create failed, ret=" << poll_fd_
                << std::endl;
      return -1;
    }

    std::cout << LMSG << "kqueue create success. poll_fd_=" << poll_fd_
              << std::endl;
  }
#else
  if (poll_fd_ < 0) {
    poll_fd_ = epoll_create(1024);

    if (poll_fd_ < 0) {
      std::cout << LMSG << "epoll_create failed, ret=" << poll_fd_ << std::endl;
      return -1;
    }

    std::cout << LMSG << "epoll_create success. poll_fd_=" << poll_fd_
              << std::endl;
  }
#endif

  return 0;
}

void Epoller::RunIOLoop(const int& timeout_in_millsecond) {
  while (!quit_) {
    WaitIO(timeout_in_millsecond);
  }
}

int Epoller::AddFd(Fd* fd) {
#if defined(__APPLE__)
  struct kevent ke;

  uint32_t events = fd->events();
  int ret = 0;
  if (events & POLLIN) {
    EV_SET(&ke, fd->fd(), EVFILT_READ, EV_ADD, 0, 0, fd);
    if ((ret = kevent(poll_fd_, &ke, 1, NULL, 0, NULL)) < 0) {
      return ret;
    }
  }
  if (events & POLLOUT) {
    EV_SET(&ke, fd->fd(), EVFILT_WRITE, EV_ADD, 0, 0, fd);
    if ((ret = kevent(poll_fd_, &ke, 1, NULL, 0, NULL)) < 0) {
      return ret;
    }
  }
#else
  struct epoll_event event;
  event.events = fd->events();
  event.data.ptr = (void*)fd;

  int ret = epoll_ctl(poll_fd_, EPOLL_CTL_ADD, fd->fd(), &event);

  if (ret < 0) {
    std::cout << LMSG << "epoll_ctl faield ret=" << ret << std::endl;
  }
#endif

  return ret;
}

int Epoller::DelFd(Fd* fd) {
#if defined(__APPLE__)
  struct kevent ke;

  uint32_t events = fd->events();
  int ret = 0;
  if (events & POLLIN) {
    EV_SET(&ke, fd->fd(), EVFILT_READ, EV_DELETE, 0, 0, fd);
    if ((ret = kevent(poll_fd_, &ke, 1, NULL, 0, NULL)) < 0) {
      return ret;
    }
  }
  if (events & POLLOUT) {
    EV_SET(&ke, fd->fd(), EVFILT_WRITE, EV_DELETE, 0, 0, fd);
    if ((ret = kevent(poll_fd_, &ke, 1, NULL, 0, NULL)) < 0) {
      return ret;
    }
  }
#else
  struct epoll_event event;
  event.events = fd->events();
  event.data.ptr = (void*)fd;

  int ret = epoll_ctl(poll_fd_, EPOLL_CTL_DEL, fd->fd(), &event);

  if (ret < 0) {
    std::cout << LMSG << "epoll_ctl failed, ret=" << ret << std::endl;
  }
#endif

  return ret;
}

int Epoller::ModFd(Fd* fd) {
#if defined(__APPLE__)
  struct kevent ke;

  uint32_t events = fd->events();
  int ret = 0;
  if (events & POLLIN) {
    EV_SET(&ke, fd->fd(), EVFILT_READ, EV_ADD, 0, 0, fd);
    if ((ret = kevent(poll_fd_, &ke, 1, NULL, 0, NULL)) < 0) {
      return ret;
    }
  }
  if (events & POLLOUT) {
    EV_SET(&ke, fd->fd(), EVFILT_WRITE, EV_ADD, 0, 0, fd);
    if ((ret = kevent(poll_fd_, &ke, 1, NULL, 0, NULL)) < 0) {
      return ret;
    }
  }
#else
  struct epoll_event event;
  event.events = fd->events();
  event.data.ptr = (void*)fd;

  int ret = epoll_ctl(poll_fd_, EPOLL_CTL_MOD, fd->fd(), &event);

  if (ret < 0) {
    std::cout << LMSG << "epoll_ctl failed, ret=" << ret << std::endl;
  }
#endif

  return ret;
}

void Epoller::WaitIO(const int& timeout_in_millsecond) {
#if defined(__APPLE__)
  static struct kevent events[1024];
  struct timespec timeout;
  timeout.tv_sec = timeout_in_millsecond / 1000;
  timeout.tv_nsec = (timeout_in_millsecond % 1000) * 1000000;
  int num_event = kevent(poll_fd_, NULL, 0, events, sizeof(events), &timeout);

  if (num_event > 0) {
    for (int i = 0; i < num_event; i++) {
      int mask = 0;
      struct kevent* e = events + i;
      Fd* fd = (Fd*)(e->udata);

      if (e->filter == EVFILT_READ) {
        int ret = fd->OnRead();
        if (ret == kClose || ret == kError) {
          std::cout << LMSG << "closed, ret:" << ret << std::endl;
          delete fd;
          return;
        }
      }
      if (e->filter == EVFILT_WRITE) {
        int ret = fd->OnWrite();
        if (ret < 0) {
          delete fd;
        }
      }
    }
  }
#else
  static epoll_event events[1024];

  int num_event =
      epoll_wait(poll_fd_, events, sizeof(events), timeout_in_millsecond);

  if (num_event > 0) {
    for (int i = 0; i < num_event; ++i) {
      Fd* fd = (Fd*)(events[i].data.ptr);

      if (fd == NULL) {
        assert(false);
      }

      if (events[i].events & (EPOLLIN | EPOLLHUP)) {
        int ret = fd->OnRead();
        if (ret == kClose || ret == kError) {
          std::cout << LMSG << "closed, ret:" << ret << std::endl;
          delete fd;
          return;
        }
      }

      if (events[i].events & EPOLLOUT) {
        int ret = fd->OnWrite();
        if (ret < 0) {
          delete fd;
        }
      }
    }
  } else if (num_event < 0) {
    std::cout << LMSG << "epoll_wait failed, ret=" << num_event << std::endl;
  } else {
  }
#endif
}
