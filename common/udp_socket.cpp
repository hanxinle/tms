#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "socket_util.h"
#include "socket_handler.h"
#include "udp_socket.h"

UdpSocket::UdpSocket(IoLoop* io_loop, const int& fd, HandlerFactoryT handler_factory)
    : Fd(io_loop, fd)
    , handler_factory_(handler_factory)
{
    memset(&src_addr_, 0, sizeof(src_addr_));
    src_addr_len_ = sizeof(src_addr_);

    socket_handler_ = handler_factory_(io_loop, this);
}

UdpSocket::~UdpSocket()
{
    delete socket_handler_;
}

int UdpSocket::OnRead()
{
    while (true)
    {
        // TODO:可以重用,加reset接口
        IoBuffer io_buffer(4096);

        int bytes = io_buffer.ReadFromFdAndWrite(fd_, (sockaddr*)&src_addr_, &src_addr_len_);

        if (bytes > 0)
        {
            socket_util::SocketAddrInetToIpPort(src_addr_, client_ip_, client_port_);
            socket_handler_->HandleRead(io_buffer, *this);
        }
        else if (bytes == 0)
        {
            // impossible
            break;
        }
        else
        {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {   
                //std::cout << LMSG << "block" << std::endl;
            }

            break;
        }
    }

    // UDP always success
    return kSuccess;
}

int UdpSocket::OnWrite()
{
    return kSuccess;
}

int UdpSocket::Send(const uint8_t* data, const size_t& len)
{
    sendto(fd_, data, len, 0, (sockaddr*)&src_addr_, src_addr_len_);

    return kSuccess;
}
