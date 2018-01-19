#include "global.h"
#include "io_buffer.h"
#include "media_center_protocol.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "tcp_socket.h"

using namespace socket_util;

MediaCenterProtocol::MediaCenterProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket)
{
}

MediaCenterProtocol::~MediaCenterProtocol()
{
}

int MediaCenterProtocol::Parse(IoBuffer& io_buffer)
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

        case GetAppStreamMasterNodeRsp::protocol_id:
        {
            GetAppStreamMasterNodeRsp get_app_stream_master_node_rsp;
            get_app_stream_master_node_rsp.Read(deserialize);

            return OnGetAppStreamMasterNodeRsp(get_app_stream_master_node_rsp);
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

int MediaCenterProtocol::OnStop()
{
    return kSuccess;
}

int MediaCenterProtocol::OnConnected()
{
    cout << LMSG << endl;

	GetTcpSocket()->SetConnected();
    GetTcpSocket()->EnableRead();
    GetTcpSocket()->DisableWrite();

	return 0;
}

int MediaCenterProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    UNUSED(now_in_ms);
    UNUSED(interval);
    UNUSED(count);

    return kSuccess;
}

int MediaCenterProtocol::OnNodeRegisterRsp(const NodeRegisterRsp& node_register_rsp)
{
	ostringstream os; 

    node_register_rsp.Dump(os);

    cout << LMSG << os.str() << endl;

    return kSuccess;
}

int MediaCenterProtocol::OnGetAppStreamMasterNodeRsp(const GetAppStreamMasterNodeRsp& get_app_stream_master_node_rsp)
{
	ostringstream os; 

    get_app_stream_master_node_rsp.Dump(os);

    cout << LMSG << os.str() << endl;

    if (get_app_stream_master_node_rsp.node_info.IsValid())
    {
        g_server_mgr->ConnectServer(get_app_stream_master_node_rsp.app, 
                                   get_app_stream_master_node_rsp.stream, 
                                   IpNum2Str(get_app_stream_master_node_rsp.node_info.ip), 
                                   get_app_stream_master_node_rsp.node_info.port[0]);
    }
    else
    {
        cout << LMSG << "can't find app:" << get_app_stream_master_node_rsp.app << ",stream:" << get_app_stream_master_node_rsp.stream << " master node" << endl;
    }

    return kSuccess;
}

int MediaCenterProtocol::Send(const Rpc& rpc)
{
    Serialize serialize;

    rpc.Write(serialize);

    const uint8_t* data = serialize.GetBuf();
    int size = serialize.GetSize();

    if (GetTcpSocket()->Send(data, size) != size)
    {   
        return -1; 
    }   

    cout << LMSG << endl;

    return 0; 
}
