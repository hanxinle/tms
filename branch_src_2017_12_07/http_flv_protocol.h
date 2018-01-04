#ifndef __HTTP_FLV_PROTOCOL_H__
#define __HTTP_FLV_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "media_subscriber.h"

class Epoller;
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
    HttpFlvProtocol(Epoller* epoller, Fd* socket);
    ~HttpFlvProtocol();

    int Parse(IoBuffer& io_buffer);

    int SendFlvHeader();

    virtual int SendMediaData(const Payload& payload);
    virtual int SendAudioHeader(const string& audio_header);
    virtual int SendVideoHeader(const string& video_header);
    virtual int SendMetaData(const string& metadata);

    int SendVideo(const Payload& payload);
    int SendAudio(const Payload& payload);

    int OnStop();
    virtual int OnPendingArrive();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    string GetApp() { return app_; }
    string GetStream() { return stream_name_; }

private:
    TcpSocket* GetTcpSocket()
    {   
        return (TcpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;
    MediaPublisher* media_publisher_;

    string app_;
    string stream_name_;
    string type_;

    uint32_t pre_tag_size_;
};

#endif // __HTTP_FLV_PROTOCOL_H__
