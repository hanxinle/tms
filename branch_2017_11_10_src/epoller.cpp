#include <string.h>

#include <iostream>
#include <sstream>

#include "common_define.h"
#include "epoller.h"
#include "fd.h"

using namespace std;

static string Event2Str(const uint32_t& events)
{
    string ret = "[";

    if (events & EPOLLIN)
    {
        ret += "IN,";
    }

    if (events & EPOLLOUT)
    {
        ret += "OUT,";
    }

    ret += "]";

    return ret;
}

Epoller::Epoller()
{
    fd_ = epoll_create(1024);

    if (fd_ < 0)
    {
        cout << "epoll_create err:" << strerror(errno) << endl;
    }
}

Epoller::~Epoller()
{
    close(fd_);
}

int Epoller::Run()
{
    return WaitIoEvent(100);
}

int Epoller::EnableSocket(Fd* fd, const uint32_t& event)
{
    int op = 0;

    if (socket_map_.count(fd) == 0)
    {
        socket_map_[fd] = event;
        op = EPOLL_CTL_ADD;
    }
    else
    {
        if (socket_map_[fd] & event)
        {
            return 0;
        }
        else
        {
            socket_map_[fd] |= event;
            op = EPOLL_CTL_MOD;
        }
    }

    cout << LMSG << "fd:" << fd << ",fd:" << fd->GetFd() << ",op:" << op << ",events:" << Event2Str(socket_map_[fd]) << endl;

    epoll_event ep_ev = {0};

    ep_ev.events = socket_map_[fd];
    ep_ev.data.ptr = fd;

    return Ctrl(op, fd->GetFd(), ep_ev);
}

int Epoller::DisableSocket(Fd* fd, const uint32_t& event)
{
    int op = 0;
    int final_event = 0;

    if (socket_map_.count(fd) == 0)
    {
        cout << "can't find fd" << fd << endl;
        return -1;
    }

    if (socket_map_[fd] & event == 0)
    {
        cout << "fd no listen " << event << " event" << endl;
        return -1;
    }

    socket_map_[fd] &= (~event);

    op = EPOLL_CTL_MOD;
    final_event = socket_map_[fd];

    cout << LMSG << "fd:" << fd << ",fd:" << fd->GetFd() << ",op:" << op << ",events:" << Event2Str(final_event) << endl;

    if (socket_map_[fd] == 0)
    {
        op = EPOLL_CTL_DEL;
        socket_map_.erase(fd);
    }

    epoll_event ep_ev = {0};

    ep_ev.events = final_event;
    ep_ev.data.ptr = fd;

    return Ctrl(op, fd->GetFd(), ep_ev);
}

int Epoller::RemoveSocket(Fd* fd)
{
    int op = 0;
    int final_event = 0;

    if (socket_map_.count(fd) == 0)
    {
        cout << LMSG << "can't find fd" << fd << endl;
        return -1;
    }

    cout << LMSG << "remove fd:" << fd << endl;

    op = EPOLL_CTL_DEL;

    epoll_event ep_ev = {0};

    ep_ev.events = socket_map_[fd];
    ep_ev.data.ptr = fd;

    socket_map_.erase(fd);

    return Ctrl(op, fd->GetFd(), ep_ev);
}
    
int Epoller::WaitIoEvent(const uint32_t& timeout_ms)
{
    static epoll_event events[1024];

    int num_events = epoll_wait(fd_, events, sizeof(events), timeout_ms);

    map<Fd*, uint32_t> socket_event;
    if (num_events > 0)
    {
        //cout << LMSG << num_events << " event happend" << endl;

        for (size_t n = 0; n != num_events; ++n)
        {
            Fd* fd = (Fd*)events[n].data.ptr;
            socket_event[fd] = events[n].events;
        }
    }
    else if (num_events == 0)
    {
        //DumpSocketMap();
    }
    else
    {
        cout << "epoll_wait err:" << strerror(errno) << endl;
    }

    HandleEvent(socket_event);

    return num_events;
}

void Epoller::HandleEvent(map<Fd*, uint32_t>& socket_event)
{
    auto iter = socket_event.begin();

    while (iter != socket_event.end())
    {
        Fd* fd = iter->first;

        if (fd == NULL)
        {
            ++iter;
            continue;
        }


        if (iter->second & EPOLLIN)
        {
            int ret = fd->OnRead();
            if (ret == kClose || ret == kError)
            {
                delete fd;
                ++iter;
                continue;
            }
        }

        // FIXME: fd delte in function[OnRead]

        if (iter->second & EPOLLOUT)
        {
            fd->OnWrite();
        }

        ++iter;
    }
}

int Epoller::Ctrl(const int& op, const int& fd, epoll_event& ep_ev)
{
    int err = epoll_ctl(fd_, op, fd, &ep_ev);
    if (err < 0)
    {
        cout << "epoll_ctl err:" << strerror(errno) << ",fd:" << fd << endl;
    }

    return err;
}

void Epoller::DumpSocketMap()
{
    ostringstream os;
    os << "socket_map_.size():" << socket_map_.size() << endl;
    for (const auto& kv : socket_map_)
    {
        Fd* fd = kv.first;
        const uint32_t& event = kv.second;
        os << fd << "=>" << Event2Str(event) << endl;
    }

    cout << LMSG << endl;
    cout << os.str() << endl;
    cout << TRACE << endl;
}
