#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "socket_util.h"
#include "socket_handler.h"
#include "tcp_socket.h"

TcpSocket::TcpSocket(IoLoop* io_loop, const int& fd, HandlerFactoryT handler_factory)
    : Fd(io_loop, fd)
    , server_socket_(false)
    , handler_factory_(handler_factory)
{
    socket_handler_ = handler_factory_(io_loop, this);
}

TcpSocket::~TcpSocket()
{
    delete socket_handler_;
}

int TcpSocket::OnRead()
{
    if (server_socket_)
    {
        std::string client_ip;
        uint16_t client_port;

        int client_fd = socket_util::Accept(fd_, client_ip, client_port);

        if (client_fd > 0)
        {
            socket_util::NoCloseWait(client_fd);

            TcpSocket* tcp_socket = new TcpSocket(io_loop_, client_fd, handler_factory_);
            socket_util::SetNonBlock(client_fd);
            tcp_socket->SetConnected();
            tcp_socket->ModName("tcp " + name() + " <-> " + client_ip + ":" + Util::Num2Str(client_port));

            std::cout << LMSG << tcp_socket->name() << " accept" << std::endl;

            socket_handler_->HandleAccept(*tcp_socket);

            tcp_socket->EnableRead();
        }
    }
    else
    {
        if (connect_status_ == kConnected)
        {
            while (true)
            {
                int bytes = read_buffer_.ReadFromFdAndWrite(fd_);
                if (bytes > 0)
                {
                    int ret = socket_handler_->HandleRead(read_buffer_, *this);

                    if (ret == kClose || ret == kError)
                    {
                        std::cout << LMSG << name() << " handle error:" << ret << std::endl;
                        socket_handler_->HandleClose(read_buffer_, *this);
                        return kClose;
                    }
                }
                else if (bytes == 0)
                {
                    std::cout << LMSG << name() << " close by peer" << std::endl;

                    socket_handler_->HandleClose(read_buffer_, *this);

                    return kClose;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        break;
                    }

                    std::cout << LMSG << name() << " read err:" << strerror(errno) << std::endl;

                    socket_handler_->HandleError(read_buffer_, *this);

                    return kError;
                }
            }
        }
    }

    return kSuccess;
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
            std::cout << LMSG << name() << " write error:" << ret << std::endl;
            socket_handler_->HandleError(read_buffer_, *this);
        }

        return ret;
    }
    else if (connect_status_ == kConnecting)
    {
        int err = -1;
        if (socket_util::GetSocketError(fd_, err) != 0 || err != 0)
        {
            std::cout << LMSG << name() << " connect error:" << strerror(err) << std::endl;
            socket_handler_->HandleError(read_buffer_, *this);
        }
        else
        {
            std::cout << LMSG << name() << " connected" << std::endl;
            SetConnected();
            socket_handler_->HandleConnected(*this);

            std::string local_ip = "";
            uint16_t local_port = 0;
            socket_util::GetSocketName(fd(), local_ip, local_port);

            std::string remote_ip = "";
            uint16_t remote_port = 0;
            socket_util::GetPeerName(fd(), remote_ip, remote_port);

            ModName("tcp " + local_ip + Util::Num2Str(local_port) + " <-> " + remote_ip + ":" + Util::Num2Str(remote_port));
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
            if (ret < (int)len)
            {
                write_buffer_.Write(data + ret, len - ret);
                EnableWrite();
            }
        }
        else
        {
            // FIXME:close socket

            std::cout << LMSG << name() << " write error:" << ret << std::endl;
            socket_handler_->HandleError(read_buffer_, *this);
        }

        return ret;
    }
    else
    {
        ret = write_buffer_.Write(data, len);
    }

    // avoid warning
    return ret;
}
