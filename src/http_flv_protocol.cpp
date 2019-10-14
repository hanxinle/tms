#include <iostream>
#include <map>

#include "common_define.h"
#include "global.h"
#include "http_flv_mgr.h"
#include "http_flv_protocol.h"
#include "http_sender.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "ref_ptr.h"
#include "rtmp_protocol.h"
#include "rtmp_mgr.h"
#include "tcp_socket.h"

using namespace std;

HttpFlvProtocol::HttpFlvProtocol(Epoller* epoller, Fd* socket)
    :
    MediaSubscriber(kHttpFlv),
    epoller_(epoller),
    socket_(socket),
    media_publisher_(NULL),
    pre_tag_size_(0)
{
}

HttpFlvProtocol::~HttpFlvProtocol()
{
}

int HttpFlvProtocol::Parse(IoBuffer& io_buffer)
{
    int ret = http_parse_.Decode(io_buffer);

    if (ret == kSuccess)
    {
        if (http_parse_.IsFlvRequest(app_, stream_))
        {
            if (! app_.empty() && ! stream_.empty())
            {
                media_publisher_ = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

                if (media_publisher_ != NULL) // 本进程有流
                {
                    HttpSender http_rsp;
                    http_rsp.SetStatus("200");
                    http_rsp.SetContentType("flv");
                    http_rsp.SetKeepAlive();

	                string http_response = http_rsp.Encode();

		            GetTcpSocket()->Send((const uint8_t*)http_response.data(), http_response.size());
                    SendFlvHeader();
                    media_publisher_->AddSubscriber(this);
                }
                else
                {
					return kError;
                }
            }
            else
            {
                cout << LMSG << "no flv" << endl;

                HttpSender http_rsp;
                http_rsp.SetStatus("404");
                http_rsp.SetContentType("html");
                http_rsp.SetClose();
                http_rsp.SetContent("fuck you");

	            string http_response = http_rsp.Encode();

		        GetTcpSocket()->Send((const uint8_t*)http_response.data(), http_response.size());
            }
        }
        else
        {
            cout << LMSG << "no flv" << endl;

            HttpSender http_rsp;
            http_rsp.SetStatus("404");
            http_rsp.SetContentType("html");
            http_rsp.SetClose();
            http_rsp.SetContent("fuck you");

	        string http_response = http_rsp.Encode();

		    GetTcpSocket()->Send((const uint8_t*)http_response.data(), http_response.size());
        }
    }

    return ret;
}

int HttpFlvProtocol::SendFlvHeader()
{
    IoBuffer flv_header;

    flv_header.Write("FLV");
    flv_header.WriteU8(1);
    flv_header.WriteU8(0x05);
    flv_header.WriteU32(9);

    uint8_t* data = NULL;
    int len = flv_header.Read(data, flv_header.Size());

    socket_->Send(data, len);

    return kSuccess;
}

int HttpFlvProtocol::SendMetaData(const string& metadata)
{
    IoBuffer flv_tag;

    uint32_t data_size = metadata.size();

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(kMetaData_AMF0);

    flv_tag.WriteU24(data_size);
    flv_tag.WriteU24(0);
    flv_tag.WriteU8(0);
    flv_tag.WriteU24(0);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send((const uint8_t*)metadata.data(), metadata.size());

    pre_tag_size_ = data_size + 11;

    return kSuccess;
}

int HttpFlvProtocol::SendMediaData(const Payload& payload)
{
    if (payload.IsAudio())
    {
        return SendAudio(payload);
    }
    else if (payload.IsVideo())
    {
        return SendVideo(payload);
    }

    return -1;
}

int HttpFlvProtocol::SendVideo(const Payload& payload)
{
    IoBuffer flv_tag;

    uint32_t data_size = payload.GetAllLen() + 5/*5 bytes avc header*/;

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(kVideo);

    flv_tag.WriteU24(data_size);
    flv_tag.WriteU24((payload.GetDts32()) & 0x00FFFFFF);
    flv_tag.WriteU8((payload.GetDts32() >> 24) & 0xFF);
    flv_tag.WriteU24(0);

    if (payload.IsIFrame())
    {
        cout << LMSG << "I frame" << endl;
        flv_tag.WriteU8(0x17);
    }
    else
    {
        flv_tag.WriteU8(0x27);
    }

    flv_tag.WriteU8(0x01); // AVC nalu

    uint32_t compositio_time_offset = payload.GetPts32() - payload.GetDts32();

    flv_tag.WriteU24(compositio_time_offset);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send(payload.GetAllData(), payload.GetAllLen());

    pre_tag_size_ = data_size + 11;

    return kSuccess;
}

int HttpFlvProtocol::SendAudio(const Payload& payload)
{
    IoBuffer flv_tag;

    uint32_t data_size = payload.GetAllLen();

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(kAudio);

    flv_tag.WriteU24(data_size);
    flv_tag.WriteU24((payload.GetDts32()) & 0x00FFFFFF);
    flv_tag.WriteU8((payload.GetDts32() >> 24) & 0xFF);
    flv_tag.WriteU24(0);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send(payload.GetAllData(), payload.GetAllLen());

    pre_tag_size_ = data_size + 11;

    return kSuccess;
}

int HttpFlvProtocol::SendVideoHeader(const string& video_header)
{
    IoBuffer flv_tag;

    uint32_t data_size = video_header.size() + 5;

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(kVideo);

    flv_tag.WriteU24(data_size);
    flv_tag.WriteU24(0);
    flv_tag.WriteU8(0);
    flv_tag.WriteU24(0);

    flv_tag.WriteU8(0x17);
    flv_tag.WriteU8(0x00); // AVC header
    flv_tag.WriteU24(0x000000);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send((const uint8_t*)video_header.data(), video_header.size());

    pre_tag_size_ = data_size + 11;

    return kSuccess;
}

int HttpFlvProtocol::SendAudioHeader(const string& audio_header)
{
    IoBuffer flv_tag;

    uint32_t data_size = audio_header.size() + 2;

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(kAudio);

    flv_tag.WriteU24(data_size);
    flv_tag.WriteU24(0);
    flv_tag.WriteU8(0);
    flv_tag.WriteU24(0);

    flv_tag.WriteU8(0xAF);
    flv_tag.WriteU8(0x00);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send((const uint8_t*)audio_header.data(), audio_header.size());

    pre_tag_size_ = data_size + 11;

    return kSuccess;
}

int HttpFlvProtocol::OnStop()
{
    if (media_publisher_ != NULL)
    {
        media_publisher_->RemoveSubscriber(this);
    }

    return kSuccess;
}

int HttpFlvProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    UNUSED(interval);
    UNUSED(count);

    if (expired_time_ms_ != 0)
    {
        if (now_in_ms > expired_time_ms_)
        {
            cout << LMSG << "expired, can't find media source, app_:" << app_ << ",stream_:" << stream_ << endl;

            OnPendingArrive();
        }
    }    

    return kSuccess;
}

int HttpFlvProtocol::OnPendingArrive()
{
    cout << LMSG << "pending done" << endl;

    if (! app_.empty() && ! stream_.empty())
    {
        media_publisher_ = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

        cout << LMSG << endl;

        if (media_publisher_ != NULL) // 从MediaCenter查到了publish node
        {
            cout << LMSG << endl;

            HttpSender http_rsp;
            http_rsp.SetStatus("200");
            http_rsp.SetContentType("flv");
            http_rsp.SetKeepAlive();

	        string http_response = http_rsp.Encode();

	        GetTcpSocket()->Send((const uint8_t*)http_response.data(), http_response.size());
            SendFlvHeader();
            media_publisher_->AddSubscriber(this);

            expired_time_ms_ = 0;
        }
        else
        {
            cout << LMSG << "can't find media source, app_:" << app_ << ",stream_:" << stream_ << endl;

			ostringstream os; 

            os << "HTTP/1.1 404 Not Found\r\n"
               << "Server: trs\r\n"
               << "Connection: close\r\n"
               << "\r\n";

            GetTcpSocket()->Send((const uint8_t*)os.str().data(), os.str().size());
        }
    }

    return kSuccess;
}
