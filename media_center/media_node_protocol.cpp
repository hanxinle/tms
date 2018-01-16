#include "fd.h"
#include "io_buffer.h"
#include "protocol.h"
#include "media_node_mgr.h"
#include "media_node_protocol.h"
#include "tcp_socket.h"

using namespace protocol;

MediaNodeProtocol::MediaNodeProtocol(Epoller* epoller, Fd* socket, MediaNodeMgr* media_node_mgr)
    :
    epoller_(epoller),
    socket_(socket),
    media_node_mgr_(media_node_mgr)
{
}

MediaNodeProtocol::~MediaNodeProtocol()
{
}

int MediaNodeProtocol::Parse(IoBuffer& io_buffer)
{
    cout << LMSG << "io_buffer size:" << io_buffer.Size() << endl;
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

    cout << LMSG << Util::Bin2Hex(data, len) << endl;

    Deserialize deserialize(data, data_len);

    switch (protocol_id)
    {
        case StreamRegisterReq::protocol_id:
        {
            StreamRegisterReq stream_register_req;
            stream_register_req.Read(deserialize);

            return OnStreamRegisterReq(stream_register_req);
        }
        break;

        case GetAppStreamMasterNodeReq::protocol_id:
        {
            GetAppStreamMasterNodeReq get_app_stream_master_node_req;
            get_app_stream_master_node_req.Read(deserialize);

            return OnGetAppStreamMasterNodeReq(get_app_stream_master_node_req);
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

int MediaNodeProtocol::OnStop()
{
}

int MediaNodeProtocol::OnConnected()
{
    cout << LMSG << endl;
}

int MediaNodeProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
}

int MediaNodeProtocol::OnStreamRegisterReq(const StreamRegisterReq& stream_register_req)
{
    ostringstream os;

    stream_register_req.Dump(os);

    cout << LMSG << os.str() << endl;

    StreamRegisterRsp stream_register_rsp;

    stream_register_rsp.rsp_time = Util::GetNowMs();

    Send(stream_register_rsp);

    for (const auto& stream : stream_register_req.stream_infos)
    {
	    media_node_mgr_->InsertNodeAppStream(stream_register_req.node_info, stream, stream_register_req.role);
    }

    return 0;
}

int MediaNodeProtocol::OnGetAppStreamMasterNodeReq(const GetAppStreamMasterNodeReq& get_app_stream_master_node_req)
{
    ostringstream os;

    get_app_stream_master_node_req.Dump(os);

    cout << LMSG << os.str() << endl;

    GetAppStreamMasterNodeRsp get_app_stream_master_node_rsp;

    get_app_stream_master_node_rsp.rsp_time = Util::GetNowMs();
    get_app_stream_master_node_rsp.app = get_app_stream_master_node_req.app;
    get_app_stream_master_node_rsp.stream = get_app_stream_master_node_req.stream;

    if (! media_node_mgr_->GetAppStreamMasterNode(get_app_stream_master_node_req.app, get_app_stream_master_node_req.stream, get_app_stream_master_node_rsp.node_info))
    {
        cout << LMSG << "can't find app:" << get_app_stream_master_node_req.app << ", stream:" << get_app_stream_master_node_req.stream << " master node" << endl;
    }

    os.clear();

    get_app_stream_master_node_rsp.Dump(os);

    cout << LMSG << os.str() << endl;

    Send(get_app_stream_master_node_rsp);

    return 0;
}

int MediaNodeProtocol::Send(const Rpc& rpc)
{
    Serialize serialize;

    rpc.Write(serialize);

    const uint8_t* data = serialize.GetBuf();
    size_t size = serialize.GetSize();

    if (GetTcpSocket()->Send(data, size) != size)
    {   
        return -1; 
    }   

    cout << LMSG << endl;

    return 0;  
}
