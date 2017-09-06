#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "socket_util.h"
#include "socket_handle.h"
#include "tcp_socket.h"

using namespace std;
using namespace socket_util;

TcpSocket::TcpSocket(Epoller* epoller, const int& fd, SocketHandle* handler)
    :
    Socket(epoller, fd),
    server_socket_(false),
    handler_(handler)
{
}

TcpSocket::~TcpSocket()
{
}

int TcpSocket::OnRead()
{
    if (server_socket_)
    {
        string client_ip;
        uint16_t client_port;

        int client_fd = Accept(fd_, client_ip, client_port);

        if (client_fd > 0)
        {
            cout << LMSG << "accept " << client_ip << ":" << client_port << endl;

            TcpSocket* tcp_socket = new TcpSocket(epoller_, client_fd, handler_);

            tcp_socket->EnableRead();
        }
    }
    else
    {
        uint8_t buf[1024*64];

        int bytes = read_buffer_.ReadFromFdAndWrite(fd_);
        if (bytes > 0)
        {
            if (handler_ != NULL)
            {
                handler_->HandleRead(read_buffer_, *this);
            }
        }
        else if (bytes == 0)
        {
            cout << LMSG << "close by peer" << endl;

            return kClose;
        }
        else
        {
            cout << LMSG << "read err:" << strerror(errno) << endl;
            return kError;
        }
    }
}

int TcpSocket::OnWrite()
{
    int ret = write_buffer_.WriteToFd(fd_);

    if (write_buffer_.Empty())
    {
        DisableWrite();
    }

    if (ret < 0)
    {
        // FIXME:write err
    }

    return ret;
}

int TcpSocket::Send(const uint8_t* data, const size_t& len)
{
    int ret = -1;
    if (write_buffer_.Empty())
    {
        ret = write(fd_, data, len);

        if (ret > 0)
        {
            cout << LMSG << "direct send " << ret << " bytes" << endl;
            if (ret < len)
            {
                write_buffer_.Write(data + ret, len - ret);
                EnableWrite();
            }
        }
        else if (ret == 0)
        {
            assert(false);
        }
        else
        {
            // FIXME:close socket
        }

        return ret;
    }
    else
    {
        write_buffer_.Write(data, len);
        return len;
    }

    // avoid warning
    return ret;
}
