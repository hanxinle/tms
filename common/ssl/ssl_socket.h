#ifndef __SSL_SOCKET_H__
#define __SSL_SOCKET_H__

#include "ssl_io_buffer.h"
#include "fd.h"

#include "openssl/ssl.h"

class IoLoop;
class SocketHandler;

class SslSocket : public Fd
{
public:
    SslSocket(IoLoop* io_loop, const int& fd, HandlerFactoryT handler_factory);
    ~SslSocket();

    void AsServerSocket() { server_socket_ = true; }

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);

    void SetDisconnected()  { connect_status_ = kDisconnected; }
    void SetConnecting()    { connect_status_ = kConnecting; }
    void SetConnected()     { connect_status_ = kConnected; }
    void SetHandshakeing()  { connect_status_ = kHandshakeing; }
    void SetHandshaked()    { connect_status_ = kHandshaked; }
    void SetDisconnecting() { connect_status_ = kDisconnecting; }

private:
    int DoHandshake();
    int SetFd();

private:
    bool            server_socket_;
    SocketHandler*   handler_;
    HandlerFactoryT  handler_factory_;
    SslIoBuffer     read_buffer_;
    SslIoBuffer     write_buffer_;

    int             connect_status_;

    SSL*            ssl_;
};

#endif // __SSL_SOCKET_H__
