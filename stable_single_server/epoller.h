#ifndef __EPOLLER_H__
#define __EPOLLER_H__

#include <sys/epoll.h>

#include <map>

using std::map;

class Fd;

class Epoller
{
public:
    Epoller();
    ~Epoller();

    int Run();
    int EnableSocket(Fd* fd, const uint32_t& event);
    int DisableSocket(Fd* fd, const uint32_t& event);
    int RemoveSocket(Fd* fd);

private:
    int WaitIoEvent(const uint32_t& timeout_ms);
    void HandleEvent(map<Fd*, uint32_t>& socket_event);
    int Ctrl(const int& op, const int& fd, epoll_event& ep_ev);
    void DumpSocketMap();

private:
    int fd_;
    map<Fd*, uint32_t>  socket_map_; // Fd*:val
};

#endif // __EPOLLER_H__
