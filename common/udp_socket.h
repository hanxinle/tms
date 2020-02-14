#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__

#include "io_buffer.h"
#include "fd.h"

class IoLoop;

class UdpSocket : public Fd
{
public:
    UdpSocket(IoLoop* io_loop, const int& fd, HandlerFactoryT handler_factory);
    ~UdpSocket();

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);

    uint16_t GetClientPort() { return client_port_; }
    std::string GetClientIp() { return client_ip_; }
    sockaddr GetSrcAddr() { return src_addr_; }
    socklen_t GetSrcAddrLen() { return src_addr_len_; }
    void SetSrcAddr(sockaddr src_addr) { src_addr_ = src_addr; }
    void SetSrcAddrLen(socklen_t src_addr_len) { src_addr_len_ = src_addr_len; }

private:
    HandlerFactoryT handler_factory_;
    sockaddr src_addr_;
    socklen_t src_addr_len_;

    std::string client_ip_;
    uint16_t client_port_;
};

#endif // __UDP_SOCKET_H__
