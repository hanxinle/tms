#include "fd.h"
#include "global.h"
#include "io_buffer.h"
#include "server_mgr.h"
#include "server_protocol.h"
#include "tcp_socket.h"

ServerProtocol::ServerProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket)
{
}

ServerProtocol::~ServerProtocol()
{
    cout << LMSG << endl;
}

int ServerProtocol::Parse(IoBuffer& io_buffer)
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
        case CreateVideoTranscodeReq::protocol_id:
        {   
            CreateVideoTranscodeReq create_video_transcode;
            create_video_transcode.Read(deserialize);

            return OnCreateVideoTranscode(create_video_transcode);
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

int ServerProtocol::OnStop()
{
    return kSuccess;
}

int ServerProtocol::OnAccept()
{
    return kSuccess;
}

int ServerProtocol::OnConnected()
{
    return kSuccess;
}

int ServerProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return kSuccess;
}

int ServerProtocol::OnCreateVideoTranscode(const CreateVideoTranscodeReq& create_video_transcode)
{
    return kSuccess;
}
