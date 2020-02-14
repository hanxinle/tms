#ifndef __HTTP_FLV_PROTOCOL_H__
#define __HTTP_FLV_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "http_parse.h"
#include "media_subscriber.h"
#include "socket_handler.h"

class IoLoop;
class Fd;
class IoBuffer;
class HttpFlvMgr;
class MediaPublisher;
class Payload;
class RtmpProtocol;
class ServerProtocol;
class ServerMgr;
class RtmpMgr;
class TcpSocket;

class HttpFlvProtocol 
    : public MediaSubscriber
    , public SocketHandler
{
public:
    HttpFlvProtocol(IoLoop* io_loop, Fd* socket);
    ~HttpFlvProtocol();

	virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket) 
    { 
        return HandleClose(io_buffer, socket); 
    }

    int Parse(IoBuffer& io_buffer);

    int SendFlvHeader();

    virtual int SendMediaData(const Payload& payload);
    virtual int SendAudioHeader(const std::string& audio_header);
    virtual int SendVideoHeader(const std::string& video_header);
    virtual int SendMetaData(const std::string& metadata);

    int SendVideo(const Payload& payload);
    int SendAudio(const Payload& payload);

    virtual int OnPendingArrive();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }

    std::string GetApp() { return app_; }
    std::string GetStream() { return stream_; }

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

    uint32_t pre_tag_size_;

    HttpParse http_parse_;
};

#endif // __HTTP_FLV_PROTOCOL_H__
