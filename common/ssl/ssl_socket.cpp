#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "fd.h"
#include "socket_util.h"
#include "socket_handler.h"
#include "ssl_socket.h"

extern SSL_CTX* g_tls_ctx;

using namespace std;
using namespace socket_util;

SslSocket::SslSocket(IoLoop* io_loop, const int& fd, HandlerFactoryT handler_factory)
    : Fd(io_loop, fd)
    , server_socket_(false)
    , handler_factory_(handler_factory)
{
    assert(g_tls_ctx != NULL);
    ssl_ = SSL_new(g_tls_ctx);

    assert(ssl_ != NULL);

    cout << LMSG << "ssl_:" << ssl_ << endl;

    read_buffer_.SetSsl(ssl_);
    write_buffer_.SetSsl(ssl_);

    socket_handler_ = handler_factory_(io_loop, this);
}

SslSocket::~SslSocket()
{
}

int SslSocket::OnRead()
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

            SslSocket* ssl_socket = new SslSocket(io_loop_, client_fd, handler_factory_);
            SetNonBlock(client_fd);

            ssl_socket->SetConnected();
            ssl_socket->SetFd();
            ssl_socket->SetHandshakeing();

            socket_handler_->HandleAccept(*ssl_socket);

            ssl_socket->EnableRead();
        }
    }
    else
    {
        if (connect_status_ == kHandshaked)
        {
            while (true)
            {
                int ret = read_buffer_.ReadFromFdAndWrite(fd_);

                cout << LMSG << "ssl read ret:" << ret << ",err:" << strerror(errno) << endl;

                if (ret > 0)
                {
                    int ret = socket_handler_->HandleRead(read_buffer_, *this);

                    if (ret == kClose || ret == kError)
                    {   
                        cout << LMSG << "read error:" << ret << endl;
                        socket_handler_->HandleClose(read_buffer_, *this);
                        return kClose;
                    }
                }
                else if (ret == 0)
                {
					cout << LMSG << "close by peer" << endl;

                    socket_handler_->HandleClose(read_buffer_, *this);

                    return kClose;
                }
                else
                {
                    int err = SSL_get_error(ssl_, ret);
                    if (err == SSL_ERROR_WANT_READ)
                    {
                        break;
                    }

                    cout << LMSG << "ssl read err:" << err << endl;
                    socket_handler_->HandleError(read_buffer_, *this);

                    return kError;
                }
            }
        }
        else if (connect_status_ == kHandshakeing)
        {
            return DoHandshake();
        }
    }

    return kSuccess;
}

int SslSocket::OnWrite()
{
    if (connect_status_ == kHandshaked)
    {
        cout << LMSG << endl;
        write_buffer_.WriteToFd(fd_);

        if (write_buffer_.Empty())
        {
            cout << LMSG << endl;
            DisableWrite();
        }

        return 0;
    }
    else if (connect_status_ == kHandshakeing)
    {
        DoHandshake();
    }
    else if (connect_status_ == kConnecting)
    {
        int err = -1;
        if (GetSocketError(fd_, err) != 0 || err != 0)
        {
            cout << LMSG << "when socket connected err:" << strerror(err) << endl;
            socket_handler_->HandleError(read_buffer_, *this);
        }
        else
        {
            cout << LMSG << "connected" << endl;
            SetConnected();
            socket_handler_->HandleConnected(*this);
        }
    }

    return 0;
}

int SslSocket::Send(const uint8_t* data, const size_t& len)
{
    assert(connect_status_ == kHandshaked);
    int ret = -1;

    if (write_buffer_.Empty())
    {   
        EnableWrite();
    }   

    // FIXME: SSL暂时不能在缓冲区空的时候直接写,不然就会发不出去, 一定要放到send buffer里面, 依靠事件循环触发, 目前搞不懂是为什么
    ret = write_buffer_.Write(data, len);

    // avoid warning
    return ret;
}

int SslSocket::DoHandshake()
{
    assert(connect_status_ == kHandshakeing);

    int ret = SSL_do_handshake(ssl_);

    cout << LMSG << endl;

    if (ret == 1)
    {
        cout << LMSG << "ssl handshake done" << endl;
        SetHandshaked();

        return kSuccess;
    }
    else
    {
        int err = SSL_get_error(ssl_, ret);

        if (err == SSL_ERROR_WANT_WRITE)
        {
            cout << LMSG << endl;
            EnableWrite();
            return kNoEnoughData;
        }
        else if (err == SSL_ERROR_WANT_READ)
        {
            cout << LMSG << endl;
            EnableRead();
            return kNoEnoughData;
        }
        else
        {
            cout << LMSG << "err:" << err << endl;
            return kError;
        }
    }

    return kError;
}

int SslSocket::SetFd()
{
    SSL_set_fd(ssl_, fd_);
    SSL_set_accept_state(ssl_);

    return 0;
}
