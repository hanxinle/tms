#include "common_define.h"
#include "fd.h"
#include "srt_epoller.h"
#include "util.h"

#include <sys/epoll.h>
#include <unistd.h>

#include "srt/srt.h"

using namespace std;

static int EpollEventToSrtEvent(const uint32_t& epoll_event)
{
    int srt_event = 0;

    if (epoll_event & EPOLLIN)
    {
        srt_event |= SRT_EPOLL_IN;
    }
    if (epoll_event & EPOLLOUT)
    {
        srt_event |= SRT_EPOLL_OUT;
    }

    return srt_event;
}

static void InitSrtSocketArray(SRTSOCKET* s, const int size, const int init_val)
{
    for (int i = 0; i < size; ++i)
    {   
        s[i] = init_val;
    }   
}

SrtEpoller::SrtEpoller()
    :
    IoLoop()
{
}

SrtEpoller::~SrtEpoller()
{
    if (poll_fd_ > 0)
    {
        close(poll_fd_);
    }
}

int SrtEpoller::Create()
{
    if (poll_fd_ < 0)
    {
        poll_fd_ = srt_epoll_create();

        if (poll_fd_ == SRT_ERROR)
        {
            cout << LMSG << "srt_epoll_create failed, ret=" << poll_fd_ << endl;
            return -1;
        }

        cout << LMSG << "srt_epoll_create success. poll_fd_=" << poll_fd_ << endl;
    }

    return 0;
}

void SrtEpoller::RunIOLoop(const int& timeout_in_millsecond)
{
    while (! quit_)
    {
        WaitIO(timeout_in_millsecond);
    }
}

int SrtEpoller::AddFd(Fd* fd)
{
    cout << LMSG << "add srt socket:" << fd->fd() << endl;
    int events = EpollEventToSrtEvent(fd->events());
    int ret = srt_epoll_add_usock(poll_fd_, fd->fd(), &events);

    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_epoll_add_usock failed, ret=" << ret << endl;
    }
    else
    {
        srt_socket_map_[fd->fd()] = fd;
    }

    return ret;
}

int SrtEpoller::DelFd(Fd* fd)
{
    int ret = srt_epoll_remove_usock(poll_fd_, fd->fd());

    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_epoll_remove_usock failed, ret=" << ret << endl;
    }
    else
    {
        srt_socket_map_.erase(fd->fd());
    }

    return ret;
}

int SrtEpoller::ModFd(Fd* fd)
{
    int events = EpollEventToSrtEvent(fd->events());
    int ret = srt_epoll_update_usock(poll_fd_, fd->fd(), &events);

    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_epoll_update_usock failed, ret=" << ret << endl;
    }

    return ret;
}

void SrtEpoller::WaitIO(const int& timeout_in_millsecond)
{
	const int kWaitFdSize = 1024;
    SRTSOCKET can_read_srt_sockets[kWaitFdSize];
    InitSrtSocketArray(can_read_srt_sockets, kWaitFdSize, SRT_INVALID_SOCK);
    int can_read_srt_sockets_num = kWaitFdSize;

    SRTSOCKET can_write_srt_sockets[kWaitFdSize];
    InitSrtSocketArray(can_write_srt_sockets, kWaitFdSize, SRT_INVALID_SOCK);
    int can_write_srt_sockets_num = kWaitFdSize;

    int ret = srt_epoll_wait(poll_fd_, 
                  can_read_srt_sockets, &can_read_srt_sockets_num, 
                  can_write_srt_sockets, &can_write_srt_sockets_num,
                  timeout_in_millsecond, 
                  NULL, NULL, NULL, NULL);

    for (int i = 0; i < can_read_srt_sockets_num; ++i)
    {   
        int srt_socket = can_read_srt_sockets[i];

        if (srt_socket == SRT_INVALID_SOCK)
        {   
            continue;
        }   

        auto iter = srt_socket_map_.find(srt_socket);

        if (iter == srt_socket_map_.end())
        {   
            cout << LMSG << "srt socket:" << srt_socket << " have not added into epoller" << endl;
            continue;
        }   

        Fd* fd = iter->second;
        if (fd == NULL)
        {   
            continue;
        }

        SRT_SOCKSTATUS srt_status = srt_getsockstate(srt_socket);

        if (srt_status == SRTS_CLOSED || srt_status == SRTS_BROKEN)
        {
            cout << LMSG << "srt socket:" << srt_socket << ", srt_status:" << (int)srt_status << endl;
            delete fd;
        }
        else
        {
            fd->OnRead();
        }
	}

    for (int i = 0; i < can_write_srt_sockets_num; ++i)
    {   
        int srt_socket = can_write_srt_sockets[i];

        if (srt_socket == SRT_INVALID_SOCK)
        {   
            continue;
        }   

        auto iter = srt_socket_map_.find(srt_socket);

        if (iter == srt_socket_map_.end())
        {   
            cout << LMSG << "srt socket:" << srt_socket << " have not added into epoller" << endl;
            continue;
        }   

        Fd* fd = iter->second;
        if (fd == NULL)
        {   
            continue;
        }

        SRT_SOCKSTATUS srt_status = srt_getsockstate(srt_socket);

        cout << LMSG << "srt socket:" << srt_socket << ", srt_status:" << (int)srt_status << endl;

        if (srt_status == SRTS_CLOSED)
        {
        }
        else
        {
            fd->OnWrite();
        }
    }
}
