#include "srt_socket.h"
#include "srt_socket_util.h"
#include "util.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "srt/srt.h"

using namespace std;

SrtSocket::SrtSocket(IoLoop* io_loop, const int& fd, SocketHandle* handle)
    :
    Fd(io_loop, fd),
    connect_status_(kDisconnected),
    handle_(handle),
    server_socket_(false)
{
}

SrtSocket::~SrtSocket()
{
}

int SrtSocket::OnRead()
{
    if (server_socket_)
    {
		sockaddr_in sa; 
        int sa_len = sizeof(sa);

        SRTSOCKET client_srt_socket = srt_accept(fd_, (sockaddr*)&sa, &sa_len);

        if (client_srt_socket != SRT_INVALID_SOCK)
        {   
            SrtSocket* srt_socket = new SrtSocket(io_loop_, client_srt_socket, handle_);
            srt_socket->SetConnected();
            srt_socket->EnableRead();

            std::string client_ip = ""; 
            uint16_t client_port = 0;

            socket_util::SocketAddrToIpPort(sa, client_ip, client_port);
            srt_socket_util::SetBlock(client_srt_socket, false);

            cout << LMSG << "accept client:" << client_ip << ":" << client_port << ", fd:" << client_srt_socket 
                    << ", streamid:" << UDT::getstreamid(client_srt_socket) << std::endl;
        }   

        return kSuccess;
    }
    else
    {
        uint8_t buf[1024*8];
        int ret = srt_recvmsg(fd(), (char*)buf, sizeof(buf));

        if (ret == SRT_ERROR)
        {
            handle_->HandleError(read_buffer_, *this);
            return kError;
        }

        //cout << LMSG << "srt recv:" << ret << " bytes, " << Util::Bin2Hex(buf, ret) << endl;

        read_buffer_.Write(buf, ret);
        ret = handle_->HandleRead(read_buffer_, *this);

		if (ret == kClose || ret == kError)
        {   
            cout << LMSG << "read error:" << ret << endl;
            handle_->HandleClose(read_buffer_, *this);
            return kClose;
        } 
    }

    return kSuccess;
}

int SrtSocket::OnWrite()
{
    if (IsConnected())
    {
        if (write_buffer_.Empty())
        {
            return 0;
        }

        int ret = write_buffer_.WriteToFd(fd_);

        cout << LMSG << "write " << ret << " nbytes" << endl;

        if (write_buffer_.Empty())
        {
            DisableWrite();
        }

        return ret;
    }

    int err = 0;
    int ret = socket_util::GetSocketError(fd_, err);

    if (ret < 0 || err < 0)
    {
        cout << LMSG << "ret:" << ret << ", err:" << err << endl;
        SetDisConnected();

        handle_->HandleError(read_buffer_, *this);
        return kError;
    }

    cout << LMSG << "connected" << endl;
    SetConnected();

    handle_->HandleConnected(*this);

    EnableRead();


    return kSuccess;
}

int SrtSocket::ConnectIp(const std::string& ip, const uint16_t& port)
{
    int ret = srt_socket_util::Connect(fd_, ip, port);
    
    if (ret < 0 && errno != EINPROGRESS)
    {
        return ret;
    }

    if (errno == EINPROGRESS)
    {
        SetConnecting();
        EnableWrite();
    }
    else
    {
        cout << LMSG << "connected immediately" << endl;
        SetConnected();
        EnableRead();
    }

    return kSuccess;
}

int SrtSocket::ConnectHost(const std::string& host, const uint16_t& port)
{
    string ip = socket_util::GetIpByHost(host);

    if (ip.empty())
    {
        return kError;
    }

    return ConnectIp(ip, port);
}

int SrtSocket::Send(const uint8_t* data, const size_t& len)
{
    int nbytes = 0;
    if (write_buffer_.Empty())
    {
        nbytes = write(fd_, data, len);

        if (nbytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            cout << LMSG << "write failed, ret=" << nbytes << endl;
            return nbytes;
        }
        
        if (nbytes > 0 && (size_t)nbytes < len)
        {
            nbytes += write_buffer_.Write(data + nbytes, len - nbytes);
            EnableWrite();
        }
    }
    else
    {
        nbytes = write_buffer_.Write(data, len);
    }

    if (write_buffer_.Size() > 0)
    {
        if (write_buffer_.WriteToFd(fd_) < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            return kError;
        }
    }

    if (write_buffer_.Empty())
    {
        DisableWrite();
    }

    return nbytes;
}
