#include "common_define.h"
#include "epoller.h"
#include "fd.h"
#include "util.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <assert.h>

#include <iostream>

using namespace std;

Epoller::Epoller()
    :
    IoLoop()
{
}

Epoller::~Epoller()
{
    if (poll_fd_ > 0)
    {
        close(poll_fd_);
    }
}

int Epoller::Create()
{
    if (poll_fd_ < 0)
    {
        poll_fd_ = epoll_create(1024);

        if (poll_fd_ < 0)
        {
            cout << LMSG << "epoll_create failed, ret=" << poll_fd_ << endl;
            return -1;
        }

        cout << LMSG << "epoll_create success. poll_fd_=" << poll_fd_ << endl;
    }

    return 0;
}

void Epoller::RunIOLoop(const int& timeout_in_millsecond)
{
    while (! quit_)
    {
        WaitIO(timeout_in_millsecond);
    }
}

int Epoller::AddFd(Fd* fd)
{
    struct epoll_event event;
    event.events = fd->events();
    event.data.ptr = (void*)fd;

    int ret = epoll_ctl(poll_fd_, EPOLL_CTL_ADD, fd->fd(), &event);

    if (ret < 0)
    {
        cout << LMSG << "epoll_ctl faield ret=" << ret << endl;
    }

    return ret;
}

int Epoller::DelFd(Fd* fd)
{
    struct epoll_event event;
    event.events = fd->events();
    event.data.ptr = (void*)fd;

    int ret = epoll_ctl(poll_fd_, EPOLL_CTL_DEL, fd->fd(), &event);

    if (ret < 0)
    {
        cout << LMSG << "epoll_ctl failed, ret=" << ret << endl;
    }

    return ret;
}

int Epoller::ModFd(Fd* fd)
{
    struct epoll_event event;
    event.events = fd->events();
    event.data.ptr = (void*)fd;

    int ret = epoll_ctl(poll_fd_, EPOLL_CTL_MOD, fd->fd(), &event);

    if (ret < 0)
    {
        cout << LMSG << "epoll_ctl failed, ret=" << ret << endl;
    }

    return ret;
}

void Epoller::WaitIO(const int& timeout_in_millsecond)
{
    static epoll_event events[1024];

    int num_event = epoll_wait(poll_fd_, events, sizeof(events), timeout_in_millsecond);

    if (num_event > 0)
    {
        for (int i = 0; i < num_event; ++i)
        {
            Fd* fd = (Fd*)(events[i].data.ptr);

            if (fd == NULL)
            {
                assert(false);
            }

            if (events[i].events & (EPOLLIN | EPOLLHUP))
            {
                int ret = fd->OnRead();
                if (ret == kClose || ret == kError)
                {
                    cout << LMSG << "closed, ret:" << ret << endl;
                    delete fd;
                    return;
                }
            }

            if (events[i].events & EPOLLOUT)
            {
                int ret = fd->OnWrite();
                if (ret < 0)
                {
                    delete fd;
                }
            }
        }
    }
    else if (num_event < 0)
    {
        cout << LMSG << "epoll_wait failed, ret=" <<num_event << endl;
    }
    else
    {
    }
}
