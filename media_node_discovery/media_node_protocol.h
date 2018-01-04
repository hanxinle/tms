#ifndef __MEDIA_NODE_PROTOCOL_H__
#define __MEDIA_NODE_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <sstream>
#include <set>

#include "socket_util.h"
#include "protocol.h"

using std::map;
using std::string;
using std::ostringstream;
using std::set;

using namespace protocol;

class Epoller;
class Fd;
class IoBuffer;
class MediaNodeMgr;
class TcpSocket;

class MediaNodeProtocol
{
public:
    MediaNodeProtocol(Epoller* epoller, Fd* socket, MediaNodeMgr* server_mgr);
    ~MediaNodeProtocol();

    int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    TcpSocket* GetTcpSocket()
    {
        return (TcpSocket*)socket_;
    }

private:
    int OnNodeRegisterReq(const NodeRegisterReq& node_register_req);
    int OnGetNodeListReq(const GetNodeListReq& get_node_list_req);

private:
    Epoller* epoller_;
    Fd* socket_;
    MediaNodeMgr* media_node_mgr_;
};

#endif // __MEDIA_NODE_PROTOCOL_H__
