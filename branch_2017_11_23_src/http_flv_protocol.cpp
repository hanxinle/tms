#include <iostream>
#include <map>

#include "common_define.h"
#include "http_flv_protocol.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "ref_ptr.h"
#include "rtmp_protocol.h"
#include "server_protocol.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"
#include "tcp_socket.h"

using namespace std;

extern LocalStreamCenter g_local_stream_center;

HttpFlvProtocol::HttpFlvProtocol(Epoller* epoller, Fd* socket, HttpFlvMgr* http_mgr, RtmpMgr* rtmp_mgr, ServerMgr* server_mgr)
    :
    epoller_(epoller),
    socket_(socket),
    http_mgr_(http_mgr),
    rtmp_mgr_(rtmp_mgr),
    media_publisher_(NULL),
    server_mgr_(server_mgr),
    pre_tag_size_(0)
{
}

HttpFlvProtocol::~HttpFlvProtocol()
{
}

int HttpFlvProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;

    int size = io_buffer.Read(data, io_buffer.Size());


    int r_pos = -1; // '\r'
    int n_pos = -1; // '\n'
    int m_pos = -1; // ':'
    int s_pos = -1; // ' '
    int pos = 0;

    bool key_value = false; // false:key, true:value

    string key;
    string value;

    map<string, string> header;

    for (int i = 0; i != size; ++i)
    {
        if (data[i] == '\r')
        {
            r_pos = i;
        }
        else if (data[i] == '\n')
        {
            if (i == r_pos + 1)
            {
                if (i == n_pos + 2) // \r\n\r\n
                {
                    cout << LMSG << "http done" << endl;

					string http_response = "HTTP/1.1 200 OK\r\n"
                                           "Server: trs\r\n"
                                           "Content-Type: flv-application/octet-stream\r\n"
                                           "Connection: keep-alive\r\n"
                                           "\r\n";

					GetTcpSocket()->Send((const uint8_t*)http_response.data(), http_response.size());

                    cout << LMSG << "app_:" << app_ << ",stream_name_:" << stream_name_ << ",type_:" << type_ << endl;
                    if (! app_.empty() && ! stream_name_.empty())
                    {
                        media_publisher_ = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_name_);

                        if (media_publisher_ != NULL)
                        {
                            if (type_ == "flv")
                            {
                                media_publisher_->AddFlvPlayer(this);
                            }
                        }
                        else
                        {
                            cout << LMSG << "can't find media source, app_:" << app_ << ",stream_name_:" << stream_name_ << endl;

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
                else // \r\n
                {
                    key_value = false;

                    cout << LMSG << key << ":" << value << endl;

                    if (key.find("GET") != string::npos)
                    {
                        //GET /test.flv HTTP/1.1
                        int s_count = 0;
                        int x_count = 0; // /
                        int d_count = 0; // .
                        for (const auto& ch : key)
                        {
                            if (ch == ' ')
                            {
                                ++s_count;
                            }
                            else if (ch == '/')
                            {
                                ++x_count;
                            }
                            else if (ch == '.')
                            {
                                ++d_count;
                            }
                            else
                            {
                                if (x_count == 1)
                                {
                                    app_ += ch;
                                }
                                else if (x_count == 2 && s_count < 2)
                                {
                                    if (d_count == 1)
                                    {
                                        type_ += ch;
                                    }
                                    else
                                    {
                                        stream_name_ += ch;
                                    }
                                }
                            }
                        }
                    }

                    header[key] = value;
                    key.clear();
                    value.clear();
                }
            }

            n_pos = i;
        }
        else if (data[i] == ':')
        {
            m_pos = i;
            key_value = true;
        }
        else if (data[i] == ' ')
        {
            if (m_pos + 1 == i)
            {
            }
            else
            {
                if (key_value)
                {
                    value += (char)data[i];
                }
                else
                {
                    key += (char)data[i];
                }
            }
        }
        else
        {
            if (key_value)
            {
                value += (char)data[i];
            }
            else
            {
                key += (char)data[i];
            }
        }
    }

    return kNoEnoughData;
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
}

int HttpFlvProtocol::SendFlvMetaData(const string& metadata)
{
    IoBuffer flv_tag;

    uint32_t data_size = metadata.size();

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(kMetaData);

    flv_tag.WriteU24(data_size);
    flv_tag.WriteU24(0);
    flv_tag.WriteU8(0);
    flv_tag.WriteU24(0);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send((const uint8_t*)metadata.data(), metadata.size());

    pre_tag_size_ = data_size + 11;
}

int HttpFlvProtocol::SendMediaData(const Payload& payload)
{
    if (payload.IsAudio())
    {
        return SendFlvAudio(payload);
    }
    else if (payload.IsVideo())
    {
        return SendFlvVideo(payload);
    }

    return -1;
}

int HttpFlvProtocol::SendFlvVideo(const Payload& payload)
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

    assert(compositio_time_offset >= 0);

    flv_tag.WriteU24(compositio_time_offset);

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send(payload.GetAllData(), payload.GetAllLen());

    pre_tag_size_ = data_size + 11;
}

int HttpFlvProtocol::SendFlvAudio(const Payload& payload)
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
}

int HttpFlvProtocol::SendFlvVideoHeader(const string& video_header)
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
}

int HttpFlvProtocol::SendFlvAudioHeader(const string& audio_header)
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
}

int HttpFlvProtocol::OnStop()
{
    if (media_publisher_ != NULL)
    {
        media_publisher_->RemoveFlvPlayer(this);
    }
}
