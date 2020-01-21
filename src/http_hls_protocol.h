#ifndef __HTTP_HLS_PROTOCOL_H__
#define __HTTP_HLS_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "media_subscriber.h"
#include "socket_handler.h"

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

class HttpHlsProtocol 
    : public MediaSubscriber
    , public SocketHandler
{
public:
    HttpHlsProtocol(IoLoop* io_loop, Fd* socket);
    ~HttpHlsProtocol();

	virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket) { return kSuccess; }
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket) 
    { 
        return HandleClose(io_buffer, socket); 
    }

    int Parse(IoBuffer& io_buffer);

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

    std::string app_;
    std::string stream_;
    std::string ts_;
    std::string type_;
};

#endif // __HTTP_HLS_PROTOCOL_H__
