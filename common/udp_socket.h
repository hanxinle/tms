#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__

#include "epoller.h"
#include "io_buffer.h"
#include "fd.h"

class Epoller;
class SocketHandle;

class UdpSocket : public Fd
{
public:
    UdpSocket(Epoller* epoller, const int& fd, SocketHandle* handler);
    ~UdpSocket();

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);
    virtual int SendTo(const uint8_t* data, const size_t& len, const string& dst_ip, const uint16_t& dst_port);

private:
    SocketHandle* handler_;
    sockaddr src_addr_;
    socklen_t src_addr_len_;
};

#endif // __UDP_SOCKET_H__
