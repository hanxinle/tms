#ifndef __MEDIA_NODE_DISCOVERY_PROTOCOL_H__
#define __MEDIA_NODE_DISCOVERY_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include "protocol.h"

using namespace protocol;

class Epoller;
class Fd;
class IoBuffer;
class MediaNodeDiscoveryMgr;
class TcpSocket;

class MediaNodeDiscoveryProtocol
{
public:
    MediaNodeDiscoveryProtocol(Epoller* epoller, Fd* socket, MediaNodeDiscoveryMgr* media_node_discovery_mgr);
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

private:
	Epoller* epoller_;
    Fd* socket_;
    MediaNodeDiscoveryMgr* media_node_discovery_mgr_;
};

#endif // __MEDIA_NODE_DISCOVERY_PROTOCOL_H__
