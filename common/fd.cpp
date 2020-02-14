#include "common_define.h"
#include "fd.h"
#include "io_loop.h"

#include <sys/epoll.h>
#include <unistd.h>

std::atomic<uint64_t> Fd::id_generator_;

Fd::Fd(IoLoop* io_loop, const int& fd)
    : events_(0)
    , fd_(fd)
    , io_loop_(io_loop)
    , socket_handler_(NULL)
    , id_(GenID())
    , name_("unknown")
{
}

Fd::~Fd()
{
    if (fd_ > 0)
    {
        DisableRead();
        DisableWrite();

        close(fd_);
    }
}

void Fd::EnableRead()
{
    if (events_ & EPOLLIN)
    {
        return;
    } 

    bool add = true;

    if (events_ != 0)
    {
        add = false;
    }

    events_ |= EPOLLIN;

    if (add)
    {
        io_loop_->AddFd(this);
    }
    else
    {
        io_loop_->ModFd(this);
    }
}

void Fd::EnableWrite()
{
    if (events_ & EPOLLOUT)
    {
        return;
    } 

    bool add = true;

    if (events_ != 0)
    {
        add = false;
    }

    events_ |= EPOLLOUT;

    if (add)
    {
        io_loop_->AddFd(this);
    }
    else
    {
        io_loop_->ModFd(this);
    }
}

void Fd::DisableRead()
{
    if ((events_ & EPOLLIN) == 0)
    {
        return;
    } 

    bool del = false;

    events_ &= (~EPOLLIN);
    if (events_ == 0)
    {
        del = true;
    }

    if (del)
    {
        io_loop_->DelFd(this);
    }
    else
    {
        io_loop_->ModFd(this);
    }
}

void Fd::DisableWrite()
{
    if ((events_ & EPOLLOUT) == 0)
    {
        return;
    } 

    bool del = false;

    events_ &= (~EPOLLOUT);
    if (events_ == 0)
    {
        del = true;
    }

    if (del)
    {
        io_loop_->DelFd(this);
    }
    else
    {
        io_loop_->ModFd(this);
    }
}
