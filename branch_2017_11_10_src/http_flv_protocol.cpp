#include <iostream>
#include <map>

#include "common_define.h"
#include "http_flv_protocol.h"
#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "stream_mgr.h"
#include "tcp_socket.h"

using namespace std;

HttpFlvProtocol::HttpFlvProtocol(Epoller* epoller, Fd* socket, HttpFlvMgr* http_mgr, StreamMgr* stream_mgr)
    :
    epoller_(epoller),
    socket_(socket),
    http_mgr_(http_mgr),
    stream_mgr_(stream_mgr),
    rtmp_src_(NULL),
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
                        rtmp_src_ = stream_mgr_->GetRtmpProtocolByAppStream(app_, stream_name_);

                        if (rtmp_src_ != NULL)
                        {
                            cout << LMSG << "rtmp_src_:" << rtmp_src_ << endl;
                            if (type_ == "flv")
                            {
                                rtmp_src_->AddFlvPlayer(this);
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

int HttpFlvProtocol::SendFlvMedia(const uint8_t& type, const bool& is_key, const uint32_t& timestamp, const uint8_t* data, const size_t& len)
{
    if (len == 0)
    {
        cout << LMSG << "!!!" << endl;
    }

    IoBuffer flv_tag;

    flv_tag.WriteU32(pre_tag_size_);

    flv_tag.WriteU8(type);

    flv_tag.WriteU24(len);
    flv_tag.WriteU24((timestamp) & 0x00FFFFFF);
    flv_tag.WriteU8((timestamp >> 24) & 0xFF);
    flv_tag.WriteU24(0);

    if (type == 9)
    {
    }
    else if (type == 8)
    {
    }
    else if (type == 18)
    {
    }

    uint8_t* buf = NULL;
    int buf_len = flv_tag.Read(buf, flv_tag.Size());

    socket_->Send(buf, buf_len);
    socket_->Send(data, len);

    pre_tag_size_ = len + 11;
}

int HttpFlvProtocol::SendPreTagSize()
{
    IoBuffer pre_tag;

    pre_tag.WriteU32(pre_tag_size_);

    uint8_t* data = NULL;
    int len = pre_tag.Read(data, pre_tag.Size());

    socket_->Send(data, len);
}

int HttpFlvProtocol::OnStop()
{
    if (rtmp_src_ != NULL)
    {
        rtmp_src_->RemoveFlvPlayer(this);
    }
}
