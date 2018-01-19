#include "fd.h"
#include "global.h"
#include "http_flv_mgr.h"
#include "http_flv_protocol.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "rtmp_protocol.h"
#include "server_mgr.h"
#include "server_protocol.h"
#include "tcp_socket.h"

ServerProtocol::ServerProtocol(Epoller* epoller, Fd* socket)
    :
    MediaSubscriber(kTcpServer),
    epoller_(epoller),
    socket_(socket),
    media_publisher_(NULL),
    role_(kUnknownServerRole)
{
}

ServerProtocol::~ServerProtocol()
{
    cout << LMSG << endl;
}

int ServerProtocol::Parse(IoBuffer& io_buffer)
{
    if (io_buffer.Size() <= kServerProtocolHeaderSize)
    {
        return kNoEnoughData;
    }

    uint32_t len = 0;

    if (io_buffer.PeekU32(len) < 0)
    {
        return kError;
    }

    //cout << LMSG << "peek len:" << len << endl;

    if (io_buffer.Size() < kServerProtocolHeaderSize + len)
    {
        return kNoEnoughData;
    }

    io_buffer.ReadU32(len);

    uint32_t protocol_id = 0;
    io_buffer.ReadU32(protocol_id);

    //cout << LMSG << "protocol_id:" << protocol_id << endl;

    if (protocol_id == kMedia)
    {
        uint32_t mask = 0;
        io_buffer.ReadU32(mask);

        uint32_t dts = 0;
        io_buffer.ReadU32(dts);

        uint32_t pts = 0;
        io_buffer.ReadU32(pts);

        uint8_t* data = (uint8_t*)malloc(len - 16);
    
        io_buffer.ReadAndCopy(data, len - 16);
    
        Payload payload(data, len - 16);

        payload.SetDts(dts);
        payload.SetPts(pts);
    
        ostringstream os;
    
        if (IsPFrame(mask))
        {
            payload.SetPFrame();
            os << "P,";
        }
        else if (IsIFrame(mask))
        {
            payload.SetIFrame();
            os << "I,";
        }
        else if (IsBFrame(mask))
        {
            payload.SetBFrame();
            os << "B,";
        }
        else if (IsHeaderFrame(mask))
        {
            os << "HEADER,";
        }
    
        if (IsAudio(mask))
        {
            os << "AUDIO,";
            payload.SetAudio();
    
            if (IsHeaderFrame(mask))
            {
                string audio_header((const char*)data, len - 16);
                media_muxer_.OnAudioHeader(audio_header);
            }
            else
            {
                media_muxer_.OnAudio(payload);

				for (auto& sub : subscriber_)
                {    
                    sub->SendMediaData(payload);
                }    
            }
        }
        else if (IsVideo(mask))
        {
            os << "VIDEO,";
            payload.SetVideo();
    
            if (IsHeaderFrame(mask))
            {
                string video_header((const char*)data, len - 16);
                media_muxer_.OnVideoHeader(video_header);
            }
            else
            {
                media_muxer_.OnVideo(payload);

				for (auto& sub : subscriber_)
                {    
                    sub->SendMediaData(payload);
                }    
            }
        }
        else if (IsMetaData(mask))
        {
            os << "METADATA,";
            string meta_data((const char*)data, len - 16);
            media_muxer_.OnMetaData(meta_data);
        }

        //cout << LMSG << "recv " << len << " bytes from server|" << os.str() << endl;
    }
    else if (protocol_id == kSetApp)
    {
        uint8_t* data = NULL;
        
        uint8_t len = 0;
        io_buffer.ReadU8(len);

        size_t str_len = (size_t)len;
        io_buffer.Read(data, str_len);

        string app((const char*)data, str_len);

        SetApp(app);

        cout << LMSG << "str_len:" << str_len << ",app:" << app_ << Util::Bin2Hex(app_) << endl;
    }
    else if (protocol_id == kSetStreamName)
    {
        uint8_t* data = NULL;
        
        uint16_t len = 0;
        io_buffer.ReadU16(len);

        size_t str_len = (size_t)len;
        io_buffer.Read(data, str_len);

        string stream((const char*)data, str_len);

        SetStreamName(stream);

        SetServerPush();

        cout << LMSG << "stream:" << stream_ << endl;

        if (app_.empty() == false && stream_.empty() == false)
        {
            g_local_stream_center.RegisterStream(app_, stream_, this, false);
        }
    }
    else if (protocol_id == kPullAppStream)
    {
        uint8_t* data = NULL;
        
        uint16_t len = 0;
        io_buffer.ReadU16(len);

        size_t str_len = (size_t)len;
        io_buffer.Read(data, str_len);

        string app((const char*)data, str_len);
        SetApp(app);

        io_buffer.ReadU16(len);
        str_len = (size_t)len;
        io_buffer.Read(data, str_len);
        string stream((const char*)data, str_len);
        SetStreamName(stream);

        // XXX: 触发slave register stream
        SendAppName();
        // XXX: 触发slave register stream
        SendStreamName();

        cout << LMSG << "app:" << app << ",stream:" << stream << endl;

        SetServerPush();

        media_publisher_ = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

        cout << LMSG "get " << app_ << ":" << stream_ << " media_publisher_:" << media_publisher_ << endl;

        if (media_publisher_ != NULL)
        {
            media_publisher_->AddSubscriber(this);

            if (media_publisher_->GetMediaMuxer().HasMetaData())
            {
                SendMetaData(media_publisher_->GetMediaMuxer().GetMetaData());
            } 
            if (media_publisher_->GetMediaMuxer().HasVideoHeader())
            {
                SendVideoHeader(media_publisher_->GetMediaMuxer().GetVideoHeader());
            }

            if (media_publisher_->GetMediaMuxer().HasAudioHeader())
            {
                SendAudioHeader(media_publisher_->GetMediaMuxer().GetAudioHeader());
            }

            auto fast_out  = media_publisher_->GetMediaMuxer().GetFastOut();
            
            for (const auto& payload : fast_out)
            {
                SendMediaData(payload);
            }
        }
    }

    return kSuccess;
}

int ServerProtocol::OnStop()
{
    cout << LMSG << endl;

    if (role_ == kServerPush)
    {
        g_local_stream_center.UnRegisterStream(app_, stream_, this);
    }

    return kSuccess;
}

int ServerProtocol::OnAccept()
{
    cout << LMSG << endl;

    return kSuccess;
}

int ServerProtocol::OnConnected()
{
    cout << LMSG << endl;

	GetTcpSocket()->SetConnected();
    GetTcpSocket()->EnableRead();
    GetTcpSocket()->DisableWrite();

    if (role_ == kPushServer_)
    {
        if (media_publisher_ != NULL)
        {
            media_publisher_->AddSubscriber(this);

            SendAppName();
            SendStreamName();

            if (media_publisher_->GetMediaMuxer().HasMetaData())
            {
                SendMetaData(media_publisher_->GetMediaMuxer().GetMetaData());
            }

            if (media_publisher_->GetMediaMuxer().HasVideoHeader())
            {
                SendVideoHeader(media_publisher_->GetMediaMuxer().GetVideoHeader());
            }

            if (media_publisher_->GetMediaMuxer().HasAudioHeader())
            {
                SendAudioHeader(media_publisher_->GetMediaMuxer().GetAudioHeader());
            }

            auto fast_out  = media_publisher_->GetMediaMuxer().GetFastOut();
            
            for (const auto& payload : fast_out)
            {
                SendMediaData(payload);
            }
        }
    }
    else if (role_ == kPullServer_)
    {
        SendPullAppStream();
    }

    return kSuccess;
}

int ServerProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    media_muxer_.EveryNSecond(now_in_ms, interval, count);

    return kSuccess;
}

int ServerProtocol::SendMediaData(const Payload& payload)
{
    IoBuffer header;

    uint32_t mask = payload.GetMask();

    header.WriteU32(payload.GetAllLen() + sizeof(uint32_t) + sizeof(mask) + sizeof(uint32_t) + sizeof(uint32_t));
    header.WriteU32(kMedia);
    header.WriteU32(mask);
    header.WriteU32(payload.GetDts());
    header.WriteU32(payload.GetPts());

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);
    socket_->Send(payload.GetAllData(), payload.GetAllLen());

    return 0;
}

int ServerProtocol::SendAudioHeader(const string& audio_header)
{
    IoBuffer header;

    uint32_t mask = 0;

    MaskAudio(mask);
    MaskHeaderFrame(mask);

    header.WriteU32(audio_header.size() + sizeof(uint32_t) + sizeof(mask) + sizeof(uint32_t) + sizeof(uint32_t));
    header.WriteU32(kMedia);
    header.WriteU32(mask);
    header.WriteU32(0);
    header.WriteU32(0);

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);
    socket_->Send((const uint8_t*)audio_header.data(), audio_header.size());

    return 0;
}

int ServerProtocol::SendVideoHeader(const string& video_header)
{
    IoBuffer header;

    uint32_t mask = 0;

    MaskVideo(mask);
    MaskHeaderFrame(mask);

    header.WriteU32(video_header.size() + sizeof(uint32_t) + sizeof(mask) + sizeof(uint32_t) + sizeof(uint32_t));
    header.WriteU32(kMedia);
    header.WriteU32(mask);
    header.WriteU32(0);
    header.WriteU32(0);

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);
    socket_->Send((const uint8_t*)video_header.data(), video_header.size());

    return 0;
}

int ServerProtocol::SendMetaData(const string& meta_data)
{
    IoBuffer header;

    uint32_t mask = 0;

    MaskMeta(mask);
    MaskHeaderFrame(mask);

    header.WriteU32(meta_data.size() + sizeof(uint32_t) + sizeof(mask) + sizeof(uint32_t) + sizeof(uint32_t));
    header.WriteU32(kMedia);
    header.WriteU32(mask);
    header.WriteU32(0);
    header.WriteU32(0);

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);
    socket_->Send((const uint8_t*)meta_data.data(), meta_data.size());

    return 0;
}

int ServerProtocol::SendAppName()
{
    IoBuffer header;

    header.WriteU32(sizeof(uint32_t) + sizeof(uint8_t) + app_.size());
    header.WriteU32(kSetApp);
    header.WriteU8(app_.size());
    header.Write(app_);

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);

    return 0;
}

int ServerProtocol::SendStreamName()
{
    IoBuffer header;

    header.WriteU32(sizeof(uint32_t) + sizeof(uint16_t) + stream_.size());
    header.WriteU32(kSetStreamName);
    header.WriteU16(stream_.size());
    header.Write(stream_);

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);

    return 0;
}

int ServerProtocol::SendPullAppStream()
{
    cout << LMSG << "app_:" << app_ << ",stream:" << stream_ << endl;
    IoBuffer header;

    header.WriteU32(sizeof(uint32_t) + sizeof(uint16_t) + app_.size() + sizeof(uint16_t) + stream_.size());
    header.WriteU32(kPullAppStream);
    header.WriteU16(app_.size());
    header.Write(app_);
    header.WriteU16(stream_.size());
    header.Write(stream_);

    uint8_t* data = NULL;

    int size = header.Read(data, header.Size());

    socket_->Send(data, size);

    return 0;
}

int ServerProtocol::OnNewRtmpPlayer(RtmpProtocol* protocol)
{
    cout << LMSG << endl;

    if (media_muxer_.HasMetaData())
    {
        protocol->SendRtmpMessage(6, 1, kMetaData, (const uint8_t*)media_muxer_.GetMetaData().data(), media_muxer_.GetMetaData().size());
    }

    if (media_muxer_.HasAudioHeader())
    {
        string audio_header;
        audio_header.append(1, 0xAF);
        audio_header.append(1, 0x00);
        audio_header.append(media_muxer_.GetAudioHeader());

        protocol->SendRtmpMessage(4, 1, kAudio, (const uint8_t*)audio_header.data(), audio_header.size());
    }

    if (media_muxer_.HasVideoHeader())
    {
        string video_header;
        video_header.append(1, 0x17);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(media_muxer_.GetVideoHeader());

        protocol->SendRtmpMessage(6, 1, kVideo, (const uint8_t*)video_header.data(), video_header.size());
    }

    auto media_fast_out = media_muxer_.GetFastOut();

    for (const auto& payload : media_fast_out)
    {
        protocol->SendMediaData(payload);
    }

    return kSuccess;
}

int ServerProtocol::OnNewFlvPlayer(HttpFlvProtocol* protocol)
{
    cout << LMSG << endl;

    protocol->SendFlvHeader();

    if (media_muxer_.HasMetaData())
    {
        protocol->SendMetaData(media_muxer_.GetMetaData());
    }

    if (media_muxer_.HasVideoHeader())
    {
        protocol->SendVideoHeader(media_muxer_.GetVideoHeader());
    }

    if (media_muxer_.HasAudioHeader())
    {
        protocol->SendAudioHeader(media_muxer_.GetAudioHeader());
    }

    auto media_fast_out = media_muxer_.GetFastOut();

    for (const auto& payload : media_fast_out)
    {
        protocol->SendMediaData(payload);
    }

    return kSuccess;
}
