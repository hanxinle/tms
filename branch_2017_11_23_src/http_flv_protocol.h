#ifndef __HTTP_FLV_PROTOCOL_H__
#define __HTTP_FLV_PROTOCOL_H__

#include <stdint.h>

#include <string>

class Epoller;
class Fd;
class IoBuffer;
class HttpFlvMgr;
class Payload;
class RtmpProtocol;
class StreamMgr;
class TcpSocket;

using std::string;

class HttpFlvProtocol
{
public:
    HttpFlvProtocol(Epoller* epoller, Fd* socket, HttpFlvMgr* http_mgr, StreamMgr* stream_mgr);
    ~HttpFlvProtocol();

    int Parse(IoBuffer& io_buffer);

    int SendFlvHeader();

    int SendFlvAudio(const Payload& payload);
    int SendFlvAudioHeader(const string& audio_header);
    int SendFlvMetaData(const string& metadata);
    int SendFlvVideo(const Payload& payload);
    int SendFlvVideoHeader(const string& video_header);

    int OnStop();

private:
    TcpSocket* GetTcpSocket()
    {   
        return (TcpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;
    HttpFlvMgr* http_mgr_;
    StreamMgr* stream_mgr_;
    RtmpProtocol* rtmp_src_;

    string app_;
    string stream_name_;
    string type_;

    uint32_t pre_tag_size_;
};

#endif // __HTTP_FLV_PROTOCOL_H__
