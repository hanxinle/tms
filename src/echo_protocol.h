#ifndef __ECHO_PROTOCOL_H__
#define __ECHO_PROTOCOL_H__

#include <stdint.h>

#include <string>

class IoLoop;
class Fd;
class IoBuffer;
class EchoMgr;
class UdpSocket;

using std::string;

class EchoProtocol
{
public:
    EchoProtocol(IoLoop* io_loop, Fd* socket);
    ~EchoProtocol();

    int Parse(IoBuffer& io_buffer);
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

	int OnStop() { return 0; }
    int OnConnected() { return 0; }

    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }

private:
    UdpSocket* GetUdpSocket()
    {   
        return (UdpSocket*)socket_;
    }

private:
    IoLoop* io_loop_;
    Fd* socket_;
};

#endif // __ECHO_PROTOCOL_H__
