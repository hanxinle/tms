#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "socket_util.h"
#include "socket_handle.h"
#include "udp_socket.h"

using namespace std;
using namespace socket_util;

UdpSocket::UdpSocket(Epoller* epoller, const int& fd, SocketHandle* handler)
    :
    Fd(epoller, fd),
    handler_(handler)
{
    memset(&src_addr_, 0, sizeof(src_addr_));
    src_addr_len_ = sizeof(src_addr_);
}

UdpSocket::~UdpSocket()
{
}

int UdpSocket::OnRead()
{
    while (true)
    {
        // TODO:可以重用,加reset接口
        IoBuffer io_buffer(4096);

        int bytes = io_buffer.ReadFromFdAndWrite(fd_, &src_addr_, &src_addr_len_);

        string udp_client_ip = "";
        uint16_t udp_client_port = 0;

        SocketAddrInetToIpPort(*(sockaddr_in*)(&src_addr_), udp_client_ip, udp_client_port);

        if (bytes > 0)
        {
            cout << LMSG << "udp recv from:" << udp_client_ip << ":" << udp_client_port << endl;

            if (handler_)
            {
                handler_->HandleRead(io_buffer, *this);
            }
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
                cout << LMSG << "block" << endl;
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
    sendto(fd_, data, len, 0, &src_addr_, src_addr_len_);

    return kSuccess;
}

int UdpSocket::SendTo(const uint8_t* data, const size_t& len, const string& dst_ip, const uint16_t& dst_port)
{
    return kSuccess;
}
