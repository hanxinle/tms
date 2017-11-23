#ifndef __HTTP_HLS_PROTOCOL_H__
#define __HTTP_HLS_PROTOCOL_H__

#include <stdint.h>

#include <string>

class Epoller;
class Fd;
class IoBuffer;
class HttpHlsMgr;
class RtmpProtocol;
class StreamMgr;
class TcpSocket;

using std::string;

class HttpHlsProtocol
{
public:
    HttpHlsProtocol(Epoller* epoller, Fd* socket, HttpHlsMgr* http_mgr, StreamMgr* stream_mgr);
    ~HttpHlsProtocol();

    int Parse(IoBuffer& io_buffer);

    int OnStop();

private:
    TcpSocket* GetTcpSocket()
    {   
        return (TcpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;
    HttpHlsMgr* http_mgr_;
    StreamMgr* stream_mgr_;
    RtmpProtocol* rtmp_src_;

    string app_;
    string stream_name_;
    string ts_;
    string type_;
};

#endif // __HTTP_HLS_PROTOCOL_H__
