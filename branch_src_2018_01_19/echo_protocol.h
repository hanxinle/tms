#ifndef __ECHO_PROTOCOL_H__
#define __ECHO_PROTOCOL_H__

#include <stdint.h>

#include <string>

class Epoller;
class Fd;
class IoBuffer;
class EchoMgr;
class UdpSocket;

using std::string;

class EchoProtocol
{
public:
    EchoProtocol(Epoller* epoller, Fd* socket);
    ~EchoProtocol();

    int Parse(IoBuffer& io_buffer);
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    UdpSocket* GetUdpSocket()
    {   
        return (UdpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;
};

#endif // __ECHO_PROTOCOL_H__
