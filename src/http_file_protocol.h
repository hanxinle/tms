#ifndef __HTTP_FILE_PROTOCOL_H__
#define __HTTP_FILE_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "http_parse.h"

class IoLoop;
class Fd;
class IoBuffer;
class TcpSocket;

using std::string;

class HttpFileProtocol
{
public:
    HttpFileProtocol(IoLoop* io_loop, Fd* socket);
    ~HttpFileProtocol();

    int Parse(IoBuffer& io_buffer);
    int Send(const uint8_t* data, const size_t& len);

    int OnStop();
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    int OnConnected() { return 0; }
    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }


private:
    TcpSocket* GetTcpSocket()
    {   
        return (TcpSocket*)socket_;
    }

private:
    IoLoop* io_loop_;
    Fd* socket_;
    HttpParse http_parse_;

    bool upgrade_;
};

#endif // __HTTP_FILE_PROTOCOL_H__
