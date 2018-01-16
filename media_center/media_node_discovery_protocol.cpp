#include "io_buffer.h"
#include "media_node_discovery_protocol.h"
#include "tcp_socket.h"

extern NodeInfo g_node_info;

MediaNodeDiscoveryProtocol::MediaNodeDiscoveryProtocol(Epoller* epoller, Fd* socket, MediaNodeDiscoveryMgr* media_node_discovery_mgr)
    :
    epoller_(epoller),
    socket_(socket),
    media_node_discovery_mgr_(media_node_discovery_mgr)
{
}

MediaNodeDiscoveryProtocol::~MediaNodeDiscoveryProtocol()
{
}

int MediaNodeDiscoveryProtocol::Parse(IoBuffer& io_buffer)
{
    cout << LMSG << endl;
	if (io_buffer.Size() <= kHeaderSize)
    {   
        return kNoEnoughData;
    }   

	uint8_t* head = NULL;
    io_buffer.Peek(head, 0, kHeaderSize);
    Deserialize peek_head(head, kHeaderSize);

    uint32_t len = 0;
    uint32_t protocol_id = 0;

    peek_head.ReadLen(len);
    peek_head.ReadProtocolId(protocol_id);

    cout << LMSG << "len:" << len << ",protocol_id:" << protocol_id << endl;

    if (io_buffer.Size() < len)
    {   
        return kNoEnoughData;
    }   

    io_buffer.Skip(kHeaderSize);
    uint8_t* data = NULL;

    uint32_t data_len = io_buffer.Read(data, len);

    Deserialize deserialize(data, data_len);

    switch (protocol_id)
    {   
        case NodeRegisterRsp::protocol_id:
        {   
            NodeRegisterRsp node_register_rsp;
            node_register_rsp.Read(deserialize);

            OnNodeRegisterRsp(node_register_rsp);
        }   
        break;

        default :
        {   
            return kError;
        }   
        break;
    }
}

int MediaNodeDiscoveryProtocol::OnStop()
{
}

int MediaNodeDiscoveryProtocol::OnConnected()
{
	GetTcpSocket()->SetConnected();
    GetTcpSocket()->EnableRead();
    GetTcpSocket()->DisableWrite();

    NodeRegisterReq node_register_req;

    node_register_req.node_info = g_node_info;

    Send(node_register_req);

	return 0;
}

int MediaNodeDiscoveryProtocol::Send(const Rpc& rpc)
{
	Serialize serialize;

    rpc.Write(serialize);

    const uint8_t* data = serialize.GetBuf();
    size_t size = serialize.GetSize();

    if (GetTcpSocket()->Send(data, size) != size)
    {   
        return -1;
    }

    return 0;
}

int MediaNodeDiscoveryProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
}

int MediaNodeDiscoveryProtocol::OnNodeRegisterRsp(const NodeRegisterRsp& node_register_rsp)
{
	ostringstream os; 

    node_register_rsp.Dump(os);

    cout << LMSG << os.str() << endl;

    return 0;
}
