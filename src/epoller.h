#ifndef __EPOLLER_H__
#define __EPOLLER_H__

#include <sys/epoll.h>

#include <map>

using std::map;

class Socket;

class Epoller
{
public:
    Epoller();
    ~Epoller();

    int Run();
    int EnableSocket(Socket* socket, const uint32_t& event);
    int DisableSocket(Socket* socket, const uint32_t& event);
    int RemoveSocket(Socket* socket);

private:
    int WaitIoEvent(const uint32_t& timeout_ms);
    void HandleEvent(map<Socket*, uint32_t>& socket_event);
    int Ctrl(const int& op, const int& fd, epoll_event& ep_ev);

private:
    int fd_;
    map<Socket*, uint32_t>  socket_map_; // socket*:val
};

#endif // __EPOLLER_H__
