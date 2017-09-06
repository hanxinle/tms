#include <string.h>

#include <iostream>

#include "common_define.h"
#include "epoller.h"
#include "socket.h"

using namespace std;

static string Evnet2Str(const uint32_t& events)
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

int Epoller::EnableSocket(Socket* socket, const uint32_t& event)
{
    int op = 0;

    if (socket_map_.count(socket) == 0)
    {
        socket_map_[socket] = event;
        op = EPOLL_CTL_ADD;
    }
    else
    {
        if (socket_map_[socket] & event == event)
        {
            return 0;
        }
        else
        {
            socket_map_[socket] |= event;
            op = EPOLL_CTL_MOD;
        }
    }

    cout << LMSG << "socket:" << socket << ",fd:" << socket->GetFd() << ",op:" << op << ",events:" << Evnet2Str(socket_map_[socket]) << endl;

    epoll_event ep_ev = {0};

    ep_ev.events = socket_map_[socket];
    ep_ev.data.ptr = socket;

    return Ctrl(op, socket->GetFd(), ep_ev);
}

int Epoller::DisableSocket(Socket* socket, const uint32_t& event)
{
    int op = 0;
    int final_event = 0;

    if (socket_map_.count(socket) == 0)
    {
        cout << "can't find socket" << socket << endl;
        return -1;
    }

    if (socket_map_[socket] & event == 0)
    {
        cout << "socket no listen " << event << " event" << endl;
        return -1;
    }

    socket_map_[socket] &= (~event);

    op = EPOLL_CTL_MOD;
    final_event = socket_map_[socket];

    if (socket_map_[socket] == 0)
    {
        op = EPOLL_CTL_DEL;
        socket_map_.erase(socket);
    }

    epoll_event ep_ev = {0};

    ep_ev.events = final_event;
    ep_ev.data.ptr = socket;

    return Ctrl(op, socket->GetFd(), ep_ev);
}

int Epoller::RemoveSocket(Socket* socket)
{
    int op = 0;
    int final_event = 0;

    if (socket_map_.count(socket) == 0)
    {
        cout << "can't find socket" << socket << endl;
        return -1;
    }

    op = EPOLL_CTL_DEL;

    epoll_event ep_ev = {0};

    ep_ev.events = socket_map_[socket];
    ep_ev.data.ptr = socket;

    socket_map_.erase(socket);

    return Ctrl(op, socket->GetFd(), ep_ev);
}
    
int Epoller::WaitIoEvent(const uint32_t& timeout_ms)
{
    static epoll_event events[1024];

    int num_events = epoll_wait(fd_, events, sizeof(events), timeout_ms);

    map<Socket*, uint32_t> socket_event;
    if (num_events > 0)
    {
        cout << LMSG << num_events << " event happend" << endl;

        for (size_t n = 0; n != num_events; ++n)
        {
            Socket* socket = (Socket*)events[n].data.ptr;
            socket_event[socket] = events[n].events;
        }
    }
    else if (num_events == 0)
    {
    }
    else
    {
        cout << "epoll_wait err:" << strerror(errno) << endl;
    }

    HandleEvent(socket_event);

    return num_events;
}

void Epoller::HandleEvent(map<Socket*, uint32_t>& socket_event)
{
    auto iter = socket_event.begin();

    while (iter != socket_event.end())
    {
        Socket* socket = iter->first;

        if (socket == NULL)
        {
            ++iter;
            continue;
        }


        if (iter->second & EPOLLIN)
        {
            int ret = socket->OnRead();
            if (ret == kClose || ret == kError)
            {
                delete socket;
                ++iter;
                continue;
            }
        }

        // FIXME: socket delte in function[OnRead]

        if (iter->second & EPOLLOUT)
        {
            socket->OnWrite();
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
