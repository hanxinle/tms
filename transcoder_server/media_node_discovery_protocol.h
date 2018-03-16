#ifndef __MEDIA_NODE_DISCOVERY_PROTOCOL_H__
#define __MEDIA_NODE_DISCOVERY_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include "protocol.h"

using namespace protocol;

class Epoller;
class Fd;
class IoBuffer;
class MediaCenterMgr;
class TcpSocket;

class MediaNodeDiscoveryProtocol
{
public:
    MediaNodeDiscoveryProtocol(Epoller* epoller, Fd* socket);
    ~MediaNodeDiscoveryProtocol();

	int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    TcpSocket* GetTcpSocket()
    {
        return (TcpSocket*)socket_;
    }

    int Send(const Rpc& rpc);

private:
    int OnNodeRegisterRsp(const NodeRegisterRsp& node_register_rsp);
    int OnGetNodeListRsp(const GetNodeListRsp& get_node_list_rsp);

private:
	Epoller* epoller_;
    Fd* socket_;
};

#endif // __MEDIA_NODE_DISCOVERY_PROTOCOL_H__
