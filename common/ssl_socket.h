#ifndef __SSL_SOCKET_H__
#define __SSL_SOCKET_H__

#include "epoller.h"
#include "ssl_io_buffer.h"
#include "fd.h"
#include "ssl_socket.h"

#include "openssl/ssl.h"

class Epoller;
class SocketHandle;

class SslSocket : public Fd
{
public:
    SslSocket(Epoller* epoller, const int& fd, SocketHandle* handler);
    ~SslSocket();

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

    void SetHandshakeing()
    {
        connect_status_ = kHandshakeing;
    }

    void SetHandshaked()
    {
        connect_status_ = kHandshaked;
    }

    void SetDisconnecting()
    {
        connect_status_ = kDisconnecting;
    }

private:
    int DoHandshake();
    int SetFd();

private:
    bool            server_socket_;
    SocketHandle*   handler_;
    SslIoBuffer     read_buffer_;
    SslIoBuffer     write_buffer_;

    int             connect_status_;

    SSL*            ssl_;
};

#endif // __SSL_SOCKET_H__
