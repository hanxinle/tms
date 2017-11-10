#ifndef __HTTP_FLV_PROTOCOL_H__
#define __HTTP_FLV_PROTOCOL_H__

#include <stdint.h>

#include <string>

class Epoller;
class Fd;
class IoBuffer;
class HttpFlvMgr;
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
    int SendFlvMedia(const uint8_t& type, const bool& is_key, const uint32_t& timestamp, const uint8_t* data, const size_t& len);
    int SendPreTagSize();

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
