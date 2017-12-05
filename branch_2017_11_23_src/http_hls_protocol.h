#ifndef __HTTP_HLS_PROTOCOL_H__
#define __HTTP_HLS_PROTOCOL_H__

#include <stdint.h>

#include <string>

class Epoller;
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

class HttpHlsProtocol
{
public:
    HttpHlsProtocol(Epoller* epoller, Fd* socket, HttpHlsMgr* http_mgr, RtmpMgr* rtmp_mgr, ServerMgr* server_mgr);
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
    RtmpMgr* rtmp_mgr_;
    ServerMgr* server_mgr_;
    MediaPublisher* media_publisher_;

    string app_;
    string stream_name_;
    string ts_;
    string type_;
};

#endif // __HTTP_HLS_PROTOCOL_H__
