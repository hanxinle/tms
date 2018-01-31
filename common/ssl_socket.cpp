#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "socket_util.h"
#include "socket_handle.h"
#include "ssl_socket.h"

extern SSL_CTX* g_ssl_ctx;

using namespace std;
using namespace socket_util;

SslSocket::SslSocket(Epoller* epoller, const int& fd, SocketHandle* handler)
    :
    Fd(epoller, fd),
    server_socket_(false),
    handler_(handler)
{
    assert(g_ssl_ctx != NULL);
    ssl_ = SSL_new(g_ssl_ctx);

    assert(ssl_ != NULL);

    cout << LMSG << "ssl_:" << ssl_ << endl;

    read_buffer_.SetSsl(ssl_);
    write_buffer_.SetSsl(ssl_);
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

            SslSocket* ssl_socket = new SslSocket(epoller_, client_fd, handler_);
            SetNonBlock(client_fd);

            ssl_socket->SetConnected();
            ssl_socket->SetFd();
            ssl_socket->SetHandshakeing();

            handler_->HandleAccept(*ssl_socket);

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
                else if (ret == 0)
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
                    int err = SSL_get_error(ssl_, ret);
                    if (err == SSL_ERROR_WANT_READ)
                    {
                        break;
                    }

                    // XXX:DEBUG CODE
                    // char buf[1024];
                    // int bytes = read(fd_, buf, sizeof(buf));
                    // cout << LMSG << "bytes:" << bytes << ",err:" << strerror(errno) << endl;

                    cout << LMSG << "ssl read err:" << err << endl;
					if (handler_ != NULL)
                    {   
                        handler_->HandleError(read_buffer_, *this);
                    }

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

int SslSocket::Send(const uint8_t* data, const size_t& len)
{
    assert(connect_status_ == kHandshaked);
    int ret = -1;

#if 0 // 下面的写法会造成when write would block, 再次发送就发不出去了
    if (write_buffer_.Empty())
    {
        ret = SSL_write(ssl_, data, len);

        cout << LMSG << "ssl write " << len << " ret:" << ret << ",err:" << strerror(errno) << endl;

        if (ret > 0)
        {
            //VERBOSE << LMSG << "direct send " << ret << " bytes" << ",left:" << (len - ret) << " bytes" << endl;

            if (ret < (int)len)
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
            int err = SSL_get_error(ssl_, ret);
            cout << LMSG << "ssl write err:" << err << endl;
            cout << LMSG << "buffer size:" << write_buffer_.Size() << endl;

            if (err == SSL_ERROR_WANT_WRITE)
            {
                write_buffer_.Write(data, len);
                EnableWrite();
                cout << LMSG << "buffer size:" << write_buffer_.Size() << endl;
                cout << LMSG << "peek:" << Util::Bin2Hex(data, 10) << endl;
            }
        }

        return ret;
    }
    else
    {
        if (write_buffer_.Empty())
        {
            EnableWrite();
        }
        cout << LMSG << "buffer size:" << write_buffer_.Size() << endl;
        ret = write_buffer_.Write(data, len);
        cout << LMSG << "buffer size:" << write_buffer_.Size() << endl;
    }
#else
    if (write_buffer_.Empty())
    {   
        EnableWrite();
    }   

    // FIXME:SSL暂时不能在缓冲区空的时候直接写,不然就会发不出去, 一定要放到send buffer里面, 依靠事件循环触发, 目前搞不懂是为什么
    ret = write_buffer_.Write(data, len);
#endif

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
