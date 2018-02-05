#ifndef __WEBRTC_PROTOCOL_H__
#define __WEBRTC_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "bit_buffer.h"

class Epoller;
class Fd;
class IoBuffer;
class WebrtcMgr;
class UdpSocket;

using std::string;

class WebrtcProtocol
{
public:
    WebrtcProtocol(Epoller* epoller, Fd* socket);
    ~WebrtcProtocol();

    int Parse(IoBuffer& io_buffer);
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    int OnStun(const uint8_t* data, const size_t& len);

private:
    UdpSocket* GetUdpSocket()
    {   
        return (UdpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;
};

#endif // __WEBRTC_PROTOCOL_H__
