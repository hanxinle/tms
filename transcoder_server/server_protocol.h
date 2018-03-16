#ifndef __SERVER_PROTOCOL_H__
#define __SERVER_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <sstream>
#include <set>

#include "protocol.h"
#include "ref_ptr.h"
#include "socket_util.h"
#include "trace_tool.h"

using namespace protocol;

using std::map;
using std::string;
using std::ostringstream;
using std::set;

class Epoller;
class Fd;
class IoBuffer;
class ServerMgr;
class TcpSocket;

class ServerProtocol
{
public:
    ServerProtocol(Epoller* epoller, Fd* socket);
    ~ServerProtocol();

    int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();
    int OnAccept();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    TcpSocket* GetTcpSocket()
    {
        return (TcpSocket*)socket_;
    }

private:
    int OnCreateVideoTranscode(const CreateVideoTranscodeReq& create_video_transcode);

private:
    Epoller* epoller_;
    Fd* socket_;
};

#endif // __SERVER_PROTOCOL_H__
