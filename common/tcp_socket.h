#ifndef __TCP_SOCKET_H__
#define __TCP_SOCKET_H__

#include <functional>

#include "io_loop.h"
#include "io_buffer.h"
#include "fd.h"

class IoLoop;
class SocketHandler;

class TcpSocket : public Fd
{
public:
    TcpSocket(IoLoop* io_loop, const int& fd, HandlerFactoryT handler_factory);
    ~TcpSocket();

    void AsServerSocket()
    {
        server_socket_ = true;
    }

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);

    void SetDisconnected()
    {
        connect_status_ = kDisconnected;
    }

    void SetConnecting()
    {
        connect_status_ = kConnecting;
    }

    void SetConnected()
    {
        connect_status_ = kConnected;
    }

    void SetDisconnecting()
    {
        connect_status_ = kDisconnecting;
    }

    SocketHandler* GetHandler()
    {
        return handler_;
    }

private:
    bool            server_socket_;
    SocketHandler*   handler_;
    IoBuffer        read_buffer_;
    IoBuffer        write_buffer_;

    int             connect_status_;

    HandlerFactoryT  handler_factory_;
};

#endif // __TCP_SOCKET_H__
