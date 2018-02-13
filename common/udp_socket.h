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

    uint16_t GetClientPort()
    {
        return client_port_;
    }

    string GetClientIp()
    {
        return client_ip_;
    }

    sockaddr GetSrcAddr()
    {
        return src_addr_;
    }

    socklen_t GetSrcAddrLen()
    {
        return src_addr_len_;
    }

    void SetSrcAddr(sockaddr src_addr)
    {
        src_addr_ = src_addr;
    }

    void SetSrcAddrLen(socklen_t src_addr_len)
    {
        src_addr_len_ = src_addr_len;
    }

private:
    SocketHandle* handler_;
    sockaddr src_addr_;
    socklen_t src_addr_len_;

    string client_ip_;
    uint16_t client_port_;
};

#endif // __UDP_SOCKET_H__
