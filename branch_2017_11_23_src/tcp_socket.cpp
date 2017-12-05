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
    Fd(epoller, fd),
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

            NoCloseWait(client_fd);

            TcpSocket* tcp_socket = new TcpSocket(epoller_, client_fd, handler_);
            SetNonBlock(client_fd);
            tcp_socket->SetConnected();

            tcp_socket->EnableRead();
        }
    }
    else
    {
        if (connect_status_ == kConnected)
        {
            uint8_t buf[1024*64];

            while (true)
            {
                int bytes = read_buffer_.ReadFromFdAndWrite(fd_);
                if (bytes > 0)
                {
                    if (handler_ != NULL)
                    {
                        int ret = handler_->HandleRead(read_buffer_, *this);

                        if (ret == kClose || ret == kError)
                        {
                            cout << LMSG << "read error:" << ret << endl;
                            handler_->HandleClose(read_buffer_, *this);
                            return kClose;
                        }
                    }
                }
                else if (bytes == 0)
                {
                    cout << LMSG << "close by peer" << endl;

                    if (handler_ != NULL)
                    {
                        handler_->HandleClose(read_buffer_, *this);
                    }

                    return kClose;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        break;
                    }

                    cout << LMSG << "read err:" << strerror(errno) << endl;

                    if (handler_ != NULL)
                    {
                        handler_->HandleError(read_buffer_, *this);
                    }

                    return kError;
                }
            }
        }
    }
}

int TcpSocket::OnWrite()
{
    if (connect_status_ == kConnected)
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
    else if (connect_status_ == kConnecting)
    {
        int err = -1;
        if (GetSocketError(fd_, err) != 0 || err != 0)
        {
            cout << LMSG << "when socket connected err:" << strerror(err) << endl;
            handler_->HandleError(read_buffer_, *this);
        }
        else
        {
            cout << LMSG << "connected" << endl;
            SetConnected();
            handler_->HandleConnected(*this);
        }
    }

    return 0;
}

int TcpSocket::Send(const uint8_t* data, const size_t& len)
{
    int ret = -1;
    if (write_buffer_.Empty())
    {
        ret = write(fd_, data, len);

        if (ret > 0)
        {
            //VERBOSE << LMSG << "direct send " << ret << " bytes" << ",left:" << (len - ret) << " bytes" << endl;

            if (ret < len)
            {
                write_buffer_.Write(data + ret, len - ret);
                EnableWrite();
            }
        }
        else if (ret == 0)
        {
            if (len != 0)
            {
                assert(false);
            }
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
