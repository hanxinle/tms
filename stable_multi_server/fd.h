#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <unistd.h>

#include <iostream>

#include "common_define.h"
#include "epoller.h"

using std::cout;
using std::endl;

class Fd
{
public:
    Fd(Epoller* epoller, const int& fd)
        :
        epoller_(epoller),
        fd_(fd)
    {
    }

    virtual ~Fd()
    {
        cout << LMSG << "remove " << fd_ << " in epoller and close it" << endl;
        epoller_->RemoveSocket(this);
        close(fd_);
    }

    void SetFd(const int& fd)
    {
        fd_ = fd;
    }

    virtual int OnRead()
    {
        return 0;
    }

    virtual int OnWrite()
    {
        return 0;
    }

    virtual int EnableRead()
    {
        return epoller_->EnableSocket(this, EPOLLIN);
    }

    virtual int EnableWrite()
    {
        return epoller_->EnableSocket(this, EPOLLOUT);
    }

    virtual int DisableWrite()
    {
        return epoller_->DisableSocket(this, EPOLLOUT);
    }

    virtual int DisableRead()
    {
        return epoller_->DisableSocket(this, EPOLLIN);
    }

    int GetFd()
    {
        return fd_;
    }

    virtual int Send(const uint8_t* data, const size_t& len) = 0;

protected:
    Epoller* epoller_;
    int fd_;
};

#endif // __SOCKET_H__
