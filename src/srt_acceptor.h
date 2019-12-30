#ifndef __SRT_ACCEPTOR_H__
#define __SRT_ACCEPTOR_H__

#include "fd.h"
#include "io_loop.h"
#include "socket.h"
#include "socket_util.h"
#include "srt_socket_util.h"

#include <string>

#include "srt/srt.h"
#include "srt/udt.h"

template<typename SOCKET, typename PROTOCOL>
class SrtAcceptor : public Fd
{
public:
    SrtAcceptor(IoLoop* io_loop, const int& fd, const bool& encrypt = false)
        :
        Fd(io_loop, fd),
        encrypt_(encrypt)
    {
    }

    ~SrtAcceptor()
    {
    }

    int OnRead()
    {
		sockaddr_in sa; 
        int sa_len = sizeof(sa);

        SRTSOCKET client_srt_socket = srt_accept(fd_, (sockaddr*)&sa, &sa_len);

        if (client_srt_socket != SRT_INVALID_SOCK)
        {   
            SOCKET* socket = new SOCKET(io_loop_, client_srt_socket);
            socket->SetConnected();

            socket->SetHandle(new PROTOCOL(socket));
            socket->EnableRead();

            std::string client_ip = "";
            uint16_t client_port = 0;

			SocketUtil::SocketAddrToIpPort(sa, client_ip, client_port);
            SrtSocketUtil::SetBlock(client_srt_socket, false);

            LogInfo << "accept client:" << client_ip << ":" << client_port << ", fd:" << client_srt_socket 
                    << ", streamid:" << UDT::getstreamid(client_srt_socket) << std::endl;
        }

        return 1;
    }

    int OnWrite()
    {
        return 0;
    }

    int OnClose() { return 0; }
    int OnConnect() { return 0; }
    int OnError() { return 0; }

private:
    bool encrypt_;
};

#endif // __SRT_ACCEPTOR_H__
