#ifndef __WEB_SOCKET_PROTOCOL_H__
#define __WEB_SOCKET_PROTOCOL_H__

#include <stdint.h>

#include <string>

class Epoller;
class Fd;
class IoBuffer;
class Payload;
class TcpSocket;

using std::string;

class WebSocketProtocol
{
public:
    WebSocketProtocol(Epoller* epoller, Fd* socket);
    ~WebSocketProtocol();

    int Parse(IoBuffer& io_buffer);
    int Send(const uint8_t* data, const size_t& len);

    int OnStop();
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    TcpSocket* GetTcpSocket()
    {   
        return (TcpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;

    bool upgrade_;
};

#endif // __WEB_SOCKET_PROTOCOL_H__
