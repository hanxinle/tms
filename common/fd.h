#ifndef __FD_H__
#define __FD_H__

#include <unistd.h>
#include <stdint.h>

#include <atomic>
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
    SocketHandler* socket_handler() { return socket_handler_; }
    uint64_t id() const { return id_; }
    std::string name() const { return name_; }

    void ModName(const std::string& name)
    {
        name_ = name;
    }

    virtual int Send(const uint8_t* data, const size_t& len) { return 0; }

    static uint64_t GenID()
    {
        return  id_generator_.fetch_add(1);
    }

protected:

    uint32_t        events_;
    int             fd_;
    IoLoop*         io_loop_;
    SocketHandler*  socket_handler_;
    uint64_t        id_;
    std::string     name_;

private:
    static std::atomic<uint64_t>    id_generator_;
};

typedef std::function<SocketHandler*(IoLoop*, Fd*)> HandlerFactoryT;

#endif // __FD_H__
