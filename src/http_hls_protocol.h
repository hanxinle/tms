#ifndef __HTTP_HLS_PROTOCOL_H__
#define __HTTP_HLS_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "media_subscriber.h"

class IoLoop;
class Fd;
class IoBuffer;
class HttpHlsMgr;
class MediaPublisher;
class RtmpProtocol;
class ServerProtocol;
class ServerMgr;
class RtmpMgr;
class TcpSocket;

using std::string;

class HttpHlsProtocol : public MediaSubscriber
{
public:
    HttpHlsProtocol(IoLoop* io_loop, Fd* socket);
    ~HttpHlsProtocol();

    int Parse(IoBuffer& io_buffer);

    int OnStop();
    int OnConnected() { return 0; }
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }
    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }

private:
    TcpSocket* GetTcpSocket()
    {   
        return (TcpSocket*)socket_;
    }

private:
    IoLoop* io_loop_;
    Fd* socket_;
    MediaPublisher* media_publisher_;

    string app_;
    string stream_;
    string ts_;
    string type_;
};

#endif // __HTTP_HLS_PROTOCOL_H__
