#ifndef __HTTP_HLS_PROTOCOL_H__
#define __HTTP_HLS_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "media_subscriber.h"

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

class HttpHlsProtocol : public MediaSubscriber
{
public:
    HttpHlsProtocol(Epoller* epoller, Fd* socket);
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
    MediaPublisher* media_publisher_;

    string app_;
    string stream_;
    string ts_;
    string type_;
};

#endif // __HTTP_HLS_PROTOCOL_H__
