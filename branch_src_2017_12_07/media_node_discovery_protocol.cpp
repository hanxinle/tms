#include "global.h"
#include "io_buffer.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "media_node_discovery_protocol.h"
#include "socket_util.h"
#include "tcp_socket.h"

using namespace socket_util;

MediaNodeDiscoveryProtocol::MediaNodeDiscoveryProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket)
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

    uint8_t* data = NULL;

    uint32_t data_len = io_buffer.Read(data, len);

    cout << Util::Bin2Hex(data, data_len) << endl;

    Deserialize deserialize(data, data_len);

    switch (protocol_id)
    {   
        case NodeRegisterRsp::protocol_id:
        {   
            NodeRegisterRsp node_register_rsp;
            node_register_rsp.Read(deserialize);

            return OnNodeRegisterRsp(node_register_rsp);
        }   
        break;

        case GetNodeListRsp::protocol_id:
        {
            GetNodeListRsp get_node_list_rsp;
            if (get_node_list_rsp.Read(deserialize) == 0)
            {
                return OnGetNodeListRsp(get_node_list_rsp);
            }
        }
        break;

        default :
        {   
            return kError;
        }   
        break;
    }

    return kError;
}

int MediaNodeDiscoveryProtocol::OnStop()
{
    return kSuccess;
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
    int size = serialize.GetSize();

    if (GetTcpSocket()->Send(data, size) != size)
    {   
        return -1;
    }

    return 0;
}

int MediaNodeDiscoveryProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    UNUSED(now_in_ms);
    UNUSED(interval);
    UNUSED(count);

    return kSuccess;
}

int MediaNodeDiscoveryProtocol::OnNodeRegisterRsp(const NodeRegisterRsp& node_register_rsp)
{
	ostringstream os; 

    node_register_rsp.Dump(os);

    cout << LMSG << os.str() << endl;

    GetNodeListReq get_node_list_req;

    get_node_list_req.req_time = Util::GetNowMs();
    get_node_list_req.type = MEDIA_CENTER;
    get_node_list_req.node_number = 1;

    Send(get_node_list_req);

    return 0;
}

int MediaNodeDiscoveryProtocol::OnGetNodeListRsp(const GetNodeListRsp& get_node_list_rsp)
{
    ostringstream os;

    get_node_list_rsp.Dump(os);

    cout << LMSG << os.str() << endl;

    if (get_node_list_rsp.type == MEDIA_CENTER)
    {
        for (const auto& node : get_node_list_rsp.node_infos)
        {
            string ip = IpNum2Str(node.ip);

            cout << LMSG << "media center:" << ip << ":" << node.port[0] << endl;

            g_media_center_mgr->ConnectMediaCenter(ip, node.port[0]);
        }
    }

    return 0;
}
