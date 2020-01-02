#ifndef __HTTP_FLV_PROTOCOL_H__
#define __HTTP_FLV_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "http_parse.h"
#include "media_subscriber.h"

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

using std::string;

class HttpFlvProtocol : public MediaSubscriber
{
public:
    HttpFlvProtocol(IoLoop* io_loop, Fd* socket);
    ~HttpFlvProtocol();

    int Parse(IoBuffer& io_buffer);

    int SendFlvHeader();

    virtual int SendMediaData(const Payload& payload);
    virtual int SendAudioHeader(const string& audio_header);
    virtual int SendVideoHeader(const string& video_header);
    virtual int SendMetaData(const string& metadata);

    int SendVideo(const Payload& payload);
    int SendAudio(const Payload& payload);

    virtual int OnPendingArrive();

    int OnStop();
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

	int OnConnected() { return 0; }
    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }

    string GetApp() { return app_; }
    string GetStream() { return stream_; }

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

    uint32_t pre_tag_size_;

    HttpParse http_parse_;
};

#endif // __HTTP_FLV_PROTOCOL_H__
