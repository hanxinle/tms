#ifndef __FD_H__
#define __FD_H__

#include <unistd.h>
#include <stdint.h>

#include <functional>

class SocketHandler;
class IoLoop;

class Fd
{
public:
    explicit Fd(IoLoop* io_loop, const int& fd = -1);
    virtual ~Fd();

    void EnableRead();
    void EnableWrite();
    void DisableRead();
    void DisableWrite();

    virtual int OnRead()    = 0;
    virtual int OnWrite()   = 0;

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }

    virtual int Send(const uint8_t* data, const size_t& len) { return 0; }

protected:
    uint32_t    events_;
    int         fd_;
    IoLoop*     io_loop_;
};

typedef std::function<SocketHandler*(IoLoop*, Fd*)> HandlerFactoryT;

#endif // __FD_H__
