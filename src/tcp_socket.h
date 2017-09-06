#ifndef __TCP_SOCKET_H__
#define __TCP_SOCKET_H__

#include "epoller.h"
#include "io_buffer.h"
#include "socket.h"

class Epoller;
class SocketHandle;

class TcpSocket : public Socket
{
public:
    TcpSocket(Epoller* epoller, const int& fd, SocketHandle* handler);
    ~TcpSocket();

    void AsServerSocket()
    {
        server_socket_ = true;
    }

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);

private:
    bool server_socket_;
    SocketHandle* handler_;
    IoBuffer      read_buffer_;
    IoBuffer      write_buffer_;
};

#endif // __TCP_SOCKET_H__
