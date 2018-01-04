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
        case NodeRegisterReq::protocol_id:
        {
            NodeRegisterReq node_register_req;
            node_register_req.Read(deserialize);

            return OnNodeRegisterReq(node_register_req);
        }
        break;

        case GetNodeListReq::protocol_id:
        {
            GetNodeListReq get_node_list_req;
            get_node_list_req.Read(deserialize);

            return OnGetNodeListReq(get_node_list_req);
        }
        break;

        default :
        {
            return kError;
        }
        break;
    }
}

int MediaNodeProtocol::OnStop()
{
}

int MediaNodeProtocol::OnConnected()
{
}

int MediaNodeProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
}

int MediaNodeProtocol::OnNodeRegisterReq(const NodeRegisterReq& node_register_req)
{
    ostringstream os;

    node_register_req.Dump(os);

    cout << LMSG << os.str() << endl;

    NodeRegisterRsp node_register_rsp;

    node_register_rsp.rsp_time = Util::GetNowMs();

    Serialize serialize;

    node_register_rsp.Write(serialize);

    media_node_mgr_->InsertNode(node_register_req.node_info);

    GetTcpSocket()->Send(serialize.GetBuf(), serialize.GetSize());

    return 0;
}

int MediaNodeProtocol::OnGetNodeListReq(const GetNodeListReq& get_node_list_req)
{
    ostringstream os;

    get_node_list_req.Dump(os);

    cout << LMSG << os.str() << endl;

    GetNodeListRsp get_node_list_rsp;

    get_node_list_rsp.node_infos = media_node_mgr_->GetNode(get_node_list_req.type, get_node_list_req.node_number);
    get_node_list_rsp.rsp_time = Util::GetNowMs();
    get_node_list_rsp.type = get_node_list_req.type;

    os.clear();
    get_node_list_rsp.Dump(os);


    Serialize serialize;

    get_node_list_rsp.Write(serialize);

    cout << LMSG << os.str() << "|len:" << serialize.GetSize() << endl;
    cout << Util::Bin2Hex(serialize.GetBuf(), serialize.GetSize()) << endl;

    GetTcpSocket()->Send(serialize.GetBuf(), serialize.GetSize());


    return 0;
}
