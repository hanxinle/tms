#include <math.h>

#include <iostream>

#include "amf_0.h"
#include "any.h"
#include "assert.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "common_define.h"
#include "crc32.h"
#include "http_flv_protocol.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "rtmp_protocol.h"
#include "fd.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"
#include "tcp_socket.h"
#include "util.h"

//#define CDN "ws.upstream.huya.com"
#define CDN "tx.direct.huya.com"
//#define CDN "al.direct.huya.com"
//#define CDN "112.90.174.121"
#define CDN_PORT 1935

using namespace std;
using namespace socket_util;

using any::Any;
using any::Int;
using any::Double;
using any::String;
using any::Vector;
using any::Map;
using any::Null;

extern LocalStreamCenter g_local_stream_center;

static uint32_t s0_len = 1;
static uint32_t s1_len = 4/*time*/ + 4/*zero*/ + 1528/*random*/;
static uint32_t s2_len = 4/*time*/ + 4/*time2*/ + 1528/*random*/;


RtmpProtocol::RtmpProtocol(Epoller* epoller, Fd* fd, RtmpMgr* rtmp_mgr, ServerMgr* server_mgr)
    :
    epoller_(epoller),
    socket_(fd),
    rtmp_mgr_(rtmp_mgr),
    server_mgr_(server_mgr),
    handshake_status_(kStatus_0),
    role_(kUnknownRtmpRole),
    in_chunk_size_(128),
    out_chunk_size_(128),
    transaction_id_(0.0),
    media_publisher_(NULL),
    can_publish_(false),
    video_frame_send_(0),
    audio_frame_send_(0),
    last_video_timestamp_(0),
    last_video_timestamp_delta_(0),
    last_audio_timestamp_(0),
    last_audio_timestamp_delta_(0),
    last_video_message_length_(0),
    last_audio_message_length_(0),
    last_message_type_id_(0)
{
    cout << LMSG << endl;
}

RtmpProtocol::~RtmpProtocol()
{
    cout << LMSG << endl;
}

int RtmpProtocol::ParseRtmpUrl(const string& url, RtmpUrl& rtmp_url)
{
    size_t pre_pos = 0;
    auto pos = url.find("rtmp://");

    if (pos == std::string::npos)
    {
        return -1;
    }

    pos += 7;
    pre_pos = pos;

    pos = url.find("/", pre_pos);

    if (pos == std::string::npos)
    {
        return -1;
    }

    std::string ip_port = url.substr(pre_pos, pos - pre_pos);

    std::string ip;
    uint16_t port;
    {
        auto pos = ip_port.find(":");
        if (pos == std::string::npos)
        {
            ip = ip_port;
            port = 1935;
        }
        else
        {
            ip = ip_port.substr(0, pos);
            port = Util::Str2Num<uint16_t>(ip_port.substr(pos + 1));
        }
    }

    pos += 1;
    pre_pos = pos;

    pos = url.find("/", pre_pos);

    if (pos == std::string::npos)
    {
        return -1;
    }

    std::string app = url.substr(pre_pos, pos - pre_pos);

    pos += 1;
    pre_pos = pos;

    std::string stream_name = url.substr(pos);

    if (stream_name.empty())
    {
        return -1;
    }

    rtmp_url.ip = ip;
    rtmp_url.port = port;
    rtmp_url.app = app;
    rtmp_url.stream_name = stream_name;

    std::cout << "ip:" << ip << ",port:" << port << ",app:" << app << ",stream_name:" << stream_name << std::endl;

    return 0;
}


int RtmpProtocol::Parse(IoBuffer& io_buffer)
{
    if (handshake_status_ == kStatus_Done)
    {
        bool one_message_done = false;
        uint32_t cs_id = 0;

        if (io_buffer.Size() >= 1)
        {
            uint8_t* buf = NULL;
            io_buffer.Peek(buf, 0, 1);

            BitBuffer bit_buffer(buf, 1);
            uint8_t fmt = 0;

            uint16_t chunk_header_len = 1;
            uint32_t message_header_len = 0;

            bit_buffer.GetBits(2, fmt);
            bit_buffer.GetBits(6, cs_id);

            if (cs_id == 0)
            {
                if (io_buffer.Size() >= 2)
                {
                    io_buffer.Peek(buf, 1, 1);
                    BitBuffer bit_buffer(buf, 1);

                    bit_buffer.GetBits(8, cs_id);
                    cs_id += 64;

                    chunk_header_len = 2;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (cs_id == 1)
            {
                if (io_buffer.Size() >= 3)
                {
                    io_buffer.Peek(buf, 1, 2);
                    BitBuffer bit_buffer(buf, 2);

                    bit_buffer.GetBits(16, cs_id);
                    cs_id += 64;

                    chunk_header_len = 3;
                }
                else
                {
                    return kNoEnoughData;
                }
            }

            if (fmt == 0)
            {
                message_header_len = 11;
            }
            else if (fmt == 1)
            {
                message_header_len = 7;
            }
            else if (fmt == 2)
            {
                message_header_len = 3;
            }
            else if (fmt == 3)
            {
                message_header_len = 0;
            }

            RtmpMessage& rtmp_msg = csid_head_[cs_id];
            if (fmt == 0)
            {
                if (io_buffer.Size() >= chunk_header_len + message_header_len)
                {
                    uint32_t timestamp = 0;
                    uint32_t message_length = 0;
                    uint8_t  message_type_id = 0;
                    uint32_t message_stream_id = 0;

                    io_buffer.Peek(buf, chunk_header_len, message_header_len);
                    BitBuffer bit_buffer(buf, message_header_len);

                    bit_buffer.GetBytes(3, timestamp);
                    bit_buffer.GetBytes(3, message_length);
                    bit_buffer.GetBytes(1, message_type_id);
                    bit_buffer.GetBytes(4, message_stream_id);

                    rtmp_msg.timestamp = timestamp;
                    rtmp_msg.timestamp_calc = timestamp;
                    rtmp_msg.message_length = message_length;
                    rtmp_msg.message_type_id = message_type_id;
                    rtmp_msg.message_stream_id = be32toh(message_stream_id);
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 1)
            {
                if (io_buffer.Size() >= chunk_header_len + message_header_len)
                {
                    uint32_t timestamp_delta = 0;
                    uint32_t message_length = 0;
                    uint8_t  message_type_id = 0;

                    io_buffer.Peek(buf, chunk_header_len, message_header_len);
                    BitBuffer bit_buffer(buf, message_header_len);

                    bit_buffer.GetBytes(3, timestamp_delta);
                    bit_buffer.GetBytes(3, message_length);
                    bit_buffer.GetBytes(1, message_type_id);

                    rtmp_msg.timestamp_delta = timestamp_delta;
                    rtmp_msg.message_length = message_length;
                    rtmp_msg.message_type_id = message_type_id;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 2)
            {
                if (io_buffer.Size() >= chunk_header_len + message_header_len)
                {
                    uint32_t timestamp_delta = 0;

                    io_buffer.Peek(buf, chunk_header_len, message_header_len);
                    BitBuffer bit_buffer(buf, message_header_len);

                    bit_buffer.GetBytes(message_header_len, timestamp_delta);

                    rtmp_msg.timestamp_delta = timestamp_delta;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 3)
            {
                RtmpMessage& rtmp_msg = csid_head_[cs_id];
            }

            if (io_buffer.Size() >= chunk_header_len + message_header_len)
            {
                uint32_t read_len = rtmp_msg.message_length - rtmp_msg.len;
                if (read_len > in_chunk_size_)
                {
                    read_len = in_chunk_size_;
                }

                if (io_buffer.Size() >= chunk_header_len + message_header_len + read_len)
                {
                    if (rtmp_msg.len == 0)
                    {
                        rtmp_msg.msg = (uint8_t*)malloc(rtmp_msg.message_length);
                    }

                    io_buffer.Skip(chunk_header_len + message_header_len);
                    io_buffer.ReadAndCopy(rtmp_msg.msg + rtmp_msg.len, read_len);

                    rtmp_msg.len += read_len;

                    if (rtmp_msg.len == rtmp_msg.message_length)
                    {
                        rtmp_msg.timestamp_calc += rtmp_msg.timestamp_delta;
                        one_message_done = true;
                    }
                    else
                    {
                        return kSuccess;
                    }
                }
                else
                {
                    return kNoEnoughData;
                }
            }
        }
        else
        {
            return kNoEnoughData;
        }

        if (one_message_done)
        {
            RtmpMessage& rtmp_msg = csid_head_[cs_id];
            rtmp_msg.cs_id = cs_id;

            int ret = OnRtmpMessage(rtmp_msg);

            free(rtmp_msg.msg);

            rtmp_msg.msg = NULL;
            rtmp_msg.len = 0;

            return ret;
        }
    }
    else
    {
        if (IsClientRole())
        {
            cout << LMSG << "handshake_status_:" << handshake_status_ << ",io_buffer.Size():" << io_buffer.Size() << endl;

            if (handshake_status_ == kStatus_2)
            {
                if (io_buffer.Size() >= s0_len + s1_len + s2_len)
                {
                    io_buffer.Skip(s0_len);

                    uint32_t time;

                    uint32_t client_time = Util::GetNowMs();

                    io_buffer.ReadU32(time);

                    uint8_t* buf = NULL;

                    io_buffer.Read(buf, s1_len - 4);
                    io_buffer.Skip(s2_len);

                    io_buffer.WriteU32(client_time);
                    io_buffer.WriteU32(time);
                    io_buffer.Write(buf, 1528);

                    io_buffer.Read(buf, s2_len);

                    socket_->Send(buf, s2_len);

                    handshake_status_ = kStatus_Done;

                    SetOutChunkSize(4096);
                    SendConnect("rtmp://" + string(CDN) + "/" + app_ + "/" + stream_name_);

                    return kSuccess;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else
            {
                cout << LMSG << "error" << endl;
                return kError;
            }
        }
        else
        {
            if (handshake_status_ == kStatus_0)
            {
                if (io_buffer.Size() >= s0_len)
                {
                    uint8_t version;

                    if (io_buffer.ReadU8(version) == 1)
                    {
                        cout << LMSG << "version:" << (uint16_t)version << endl;
                        handshake_status_ = kStatus_1;
                        return kSuccess;
                    }
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (handshake_status_ == kStatus_1)
            {
                if (io_buffer.Size() >= s1_len)
                {
                    uint8_t* buf = NULL;
                    io_buffer.Read(buf, 4);

                    BitBuffer bit_buffer(buf, 4);

                    uint32_t timestamp;
                    bit_buffer.GetBytes(4, timestamp);
                    // send s0 + s1 + s2

                    io_buffer.Read(buf, 4);
                    io_buffer.Read(buf, 1528);

                    // s0
                    uint8_t version = 1;
                    io_buffer.WriteU8(version);

                    // s1
                    uint32_t server_time = Util::GetNowMs();
                    io_buffer.WriteU32(server_time);

                    uint32_t zero = 0;
                    io_buffer.WriteU32(zero);

                    io_buffer.WriteFake(1528);

                    // s2
                    io_buffer.WriteU32(timestamp);
                    io_buffer.WriteU32(server_time);
                    io_buffer.Write(buf, 1528);

                    io_buffer.WriteToFd(socket_->GetFd());

                    handshake_status_ = kStatus_2;
                    return kSuccess;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (handshake_status_ == kStatus_2)
            {
                if (io_buffer.Size() >= s2_len)
                {
                    uint8_t* buf = NULL;
                    io_buffer.Read(buf, 8);

                    BitBuffer bit_buffer(buf, 8);

                    uint32_t timestamp = 0;
                    bit_buffer.GetBytes(4, timestamp);

                    uint32_t timestamp2 = 0;
                    bit_buffer.GetBytes(4, timestamp2);

                    io_buffer.Read(buf, 1528);

                    handshake_status_ = kStatus_Done;

                    SetOutChunkSize(4096);

                    cout << LMSG << "Handshake done!!!" << endl;
                    return kSuccess;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
        }
    }

    assert(false);
    // avoid warning
    cout << LMSG << "error" << endl;
    return kError;
}

int RtmpProtocol::OnSetChunkSize(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t chunk_size = 0;
    bit_buffer.GetBytes(4, chunk_size);

    cout << LMSG << "chunk_size:" << in_chunk_size_ << "->" << chunk_size << endl;

    in_chunk_size_ = chunk_size;

    return kSuccess;
}

int RtmpProtocol::OnAcknowledgement(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t sequence_number = 0;
    bit_buffer.GetBytes(4, sequence_number);

    cout << LMSG << "sequence_number:" << sequence_number << endl;

    return kSuccess;
}

int RtmpProtocol::OnWindowAcknowledgementSize(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t ack_window_size = 0;
    bit_buffer.GetBytes(4, ack_window_size);

    cout << LMSG << "ack_window_size:" << ack_window_size << endl;

    SendUserControlMessage(0, 0);

    return kSuccess;
}

int RtmpProtocol::OnSetPeerBandwidth(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t ack_window_size = 0;
    bit_buffer.GetBytes(4, ack_window_size);

    uint8_t limit_type = 0;
    bit_buffer.GetBytes(1, limit_type);

    cout << LMSG << "ack_window_size:" << ack_window_size
                 << ", limit_type:" << (int)limit_type 
                 << endl;

    return kSuccess;
}

int RtmpProtocol::OnUserControlMessage(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint16_t event = 0xff;
    bit_buffer.GetBytes(2, event);

    uint32_t data = 0;
    bit_buffer.GetBytes(4, data);

    cout << LMSG << "user control message, event:" << event << ",data:" << data << endl;

    return kSuccess;
}

int RtmpProtocol::OnAudio(RtmpMessage& rtmp_msg)
{
    bool insert_to_aq = false;

    if (rtmp_msg.len >= 2)
    {
        BitBuffer bit_buffer(rtmp_msg.msg, 2);

        uint8_t sound_format = 0xff;
        uint8_t sound_rate = 0xff;
        uint8_t sound_size = 0xff;
        uint8_t sound_type = 0xff;
        uint8_t aac_packet_type = 0xff;

        bit_buffer.GetBits(4, sound_format);
        bit_buffer.GetBits(2, sound_rate);
        bit_buffer.GetBits(1, sound_size);
        bit_buffer.GetBits(1, sound_type);
        bit_buffer.GetBits(8, aac_packet_type);

        if (sound_format == 10)
        {
            if (aac_packet_type == 0)
            {
                string audio_header((const char*)rtmp_msg.msg + 2, rtmp_msg.len - 2);

                cout << LMSG << "recv audio_header,size:" << audio_header.size() << endl;
                cout << Util::Bin2Hex(audio_header) << endl;

                media_muxer_.OnAudioHeader(audio_header);
            }
            else
            {
                uint8_t* audio_raw_data = (uint8_t*)malloc(rtmp_msg.len);
                memcpy(audio_raw_data, rtmp_msg.msg, rtmp_msg.len);

                Payload audio_payload(audio_raw_data, rtmp_msg.len);
                audio_payload.SetAudio();
                audio_payload.SetDts(rtmp_msg.timestamp_calc);
                audio_payload.SetPts(rtmp_msg.timestamp_calc);

                media_muxer_.OnAudio(audio_payload);

                for (auto& dst : rtmp_forwards_)
                {
                    if (dst->CanPublish())
                    {
                        dst->SendMediaData(audio_payload);
                    }
                }

                for (auto& player : rtmp_player_)
                {
                    player->SendMediaData(audio_payload);
                }

                for (auto& player : flv_player_)
                {
                    player->SendFlvAudio(audio_payload);
                }

                for (auto& follow : server_follow_)
                {
                    follow->SendMediaData(audio_payload);
                }
            }
        }
    }
    else
    {
        cout << LMSG << "impossible?" << endl;
        assert(false);
    }

    return kSuccess;
}

int RtmpProtocol::OnVideo(RtmpMessage& rtmp_msg)
{
    bool to_media_muxer = false;
    uint8_t frame_type = 0xff;
    uint8_t codec_id = 0xff;
    uint8_t avc_packet_type = 0xff;

    if (rtmp_msg.len >= 2)
    {
        BitBuffer bit_buffer(rtmp_msg.msg, 2);

        bit_buffer.GetBits(4, frame_type);
        bit_buffer.GetBits(4, codec_id);
        bit_buffer.GetBits(8, avc_packet_type);

        // H264/AVC
        if (codec_id == 7)
        {
            if (avc_packet_type == 0)
            {
                OnVideoHeader(rtmp_msg);
            }
            else
            {
                uint32_t compositio_time_offset = 0;
                if (rtmp_msg.len > 5)
                {
                    compositio_time_offset = (rtmp_msg.msg[2] << 16) | (rtmp_msg.msg[3] << 8) | (rtmp_msg.msg[4]);

                    uint8_t* data = rtmp_msg.msg + 5;
                    size_t raw_len = rtmp_msg.len - 5;

                    size_t cur_len = 0;
                    while (cur_len < raw_len)
                    {
                        uint32_t nalu_len = (data[cur_len]<<24) | (data[cur_len+1]<<16) | (data[cur_len+2]<<8) | (data[cur_len+3]);

                        if (nalu_len > raw_len)
                        {
                            cout << LMSG << "nalu_len:" << nalu_len << " > raw_len:" << raw_len;
                            break;
                        }

                        uint8_t nalu_header = data[cur_len+4];

                        uint8_t forbidden_zero_bit = (nalu_header & 0x80) >> 7;
                        uint8_t nal_ref_idc = (nalu_header & 0x60) >> 5;
                        uint8_t nalu_unit_type = (nalu_header & 0x1F);

                        // 4 bytes nalu_len也push,方便后面FLV/RTMP的处理
                        uint8_t* video_raw_data = (uint8_t*)malloc(nalu_len + 4);
                        memcpy(video_raw_data, data + cur_len, nalu_len + 4);

                        Payload video_payload(video_raw_data, nalu_len + 4);
                        video_payload.SetVideo();
                        video_payload.SetDts(rtmp_msg.timestamp_calc);
                        video_payload.SetPts(rtmp_msg.timestamp_calc);

                        //cout << LMSG << "NALU type + 4byte payload peek:[" << Util::Bin2Hex(data+cur_len+4, 5) << endl;

                        if (nalu_unit_type == 6)
                        {
                            //cout << LMSG << "SEI [" << Util::Bin2Hex(data + cur_len + 4, nalu_len) << "]" << endl;
                            to_media_muxer = false;
                        }
                        else if (nalu_unit_type == 7)
                        {
                            cout << LMSG << "SPS [" << Util::Bin2Hex(data + cur_len + 4, nalu_len) << "]" << endl;
                        }
                        else if (nalu_unit_type == 8)
                        {
                            cout << LMSG << "PPS [" << Util::Bin2Hex(data + cur_len + 4, nalu_len) << "]" << endl;
                        }
                        else if (nalu_unit_type == 5)
                        {
                            to_media_muxer = true;
                            cout << LMSG << "IDR" << endl;
                            video_payload.SetIFrame();
                            video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                        }
                        else if (nalu_unit_type == 1)
                        {
                            to_media_muxer = true;

                            if (nal_ref_idc == 2)
                            {
                                //cout << LMSG << "P" << endl;
                                video_payload.SetPFrame();
                                video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                            }
                            else if (nal_ref_idc == 0)
                            {
                                //cout << LMSG << "B" << endl;
                                video_payload.SetBFrame();
                                video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                            }
                            else
                            {
                                if (compositio_time_offset == 0)
                                {
                                    //cout << LMSG << "B/P => P" << endl;
                                    video_payload.SetPFrame();
                                    video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                                }
                                else
                                {
                                    //cout << LMSG << "B/P => B" << endl;
                                    video_payload.SetBFrame();
                                    video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                                }
                            }
                        }
                        else
                        {
                            to_media_muxer = false;
                        }

                        if (to_media_muxer)
                        {
                            if (media_muxer_.GetForwardToggleBit())
                            {
                                ConnectForwardRtmpServer(CDN, CDN_PORT);
                                ConnectFollowServer("127.0.0.1", 10002);
                            }

                            media_muxer_.OnVideo(video_payload);

                            for (auto& dst : rtmp_forwards_)
                            {
                                if (dst->CanPublish())
                                {
                                    dst->SendMediaData(video_payload);
                                }
                            }

                            for (auto& player : rtmp_player_)
                            {
                                player->SendMediaData(video_payload);
                            }

                            for (auto& player : flv_player_)
                            {
                                player->SendFlvVideo(video_payload);
                            }

                            for (auto& follow : server_follow_)
                            {
                                follow->SendMediaData(video_payload);
                            }
                        }

                        cur_len += nalu_len + 4;
                    }

                    assert(cur_len == raw_len);
                }
            }
        }
    }
    else
    {
        cout << LMSG << "impossible?" << endl;
        assert(false);
    }

    return kSuccess;
}

int RtmpProtocol::OnAmf0Message(RtmpMessage& rtmp_msg)
{
    string amf((const char*)rtmp_msg.msg, rtmp_msg.len);

    AmfCommand amf_command;
    int ret = Amf0::Decode(amf,  amf_command);
    cout << LMSG << "ret:" << ret << ", amf_command.size():" <<  amf_command.size() << endl;

    for (size_t index = 0; index != amf_command.size(); ++index)
    {
        const auto& command = amf_command[index];

        if (command != NULL)
        {
            cout << LMSG << "v type:" << command->TypeStr() << endl;
        }
        else
        {
            cout << LMSG << "v NULL" << endl;
        }
    }

    if (ret == 0 &&  amf_command.size() >= 1)
    {
        string command = "";
        if ( amf_command[0]->GetString(command))
        {
            double trans_id = 0;

            cout << LMSG << "recv [" << command << " command]" << endl;

            if (IsClientRole())
            {
                cout << LMSG << "command:" << command << ",last_send_command_:" << last_send_command_ << ",transaction_id_:" << transaction_id_ << endl;
            }

            if (command == "connect")
            {
                return OnConnectCommand(amf_command);
            }
            else if (command == "play")
            {
                return OnPlayCommand(rtmp_msg, amf_command);
            }
            else if (command == "publish")
            {
                return OnPublishCommand(rtmp_msg, amf_command);
            }
            else if (command == "createStream")
            {
                return OnCreateStreamCommand(rtmp_msg, amf_command);
            }
            else if (command == "releaseStream")
            {
            }
            else if (command == "FCPublish")
            {
            }
            else if (command == "deleteStream")
            {
            }
            else if (command == "FCUnpublish")
            {
            }
            else if (command == "onFCPublish")
            {
                // XXX:还有一个_result(FCPublish这种请求有2个回包,1个是onFCPublish, 1个是_result)
                //SendCreateStream();
            }
            else if (command == "_result")
            {
                return OnResultCommand(amf_command);
            }
            else if (command == "_error")
            {
            }
            else if (command == "onStatus")
            {
                return OnStatusCommand(amf_command);
            }
        }
        else
        {
            assert(false);
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnConnectCommand(AmfCommand& amf_command)
{
    double trans_id = 0;
    map<string, Any*> command_object;

    if (amf_command.size() >= 3)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            cout << LMSG << "transaction_id:" << trans_id << endl;
        }
        if (amf_command[2]->GetMap(command_object))
        {
            for (auto& kv : command_object)
            {
                if (kv.first == "app")
                {
                    string app;
                    if (kv.second->GetString(app))
                    {
                        auto pos = app.find("/");
                        if (pos != string::npos)
                        {
                            app = app.substr(0, pos);
                        }
                        cout << LMSG << "app = " << app << endl;
                        SetApp(app);
                    }
                }

                if (kv.first == "tcUrl")
                {
                    if (kv.second->GetString(tc_url_))
                    {
                        cout << LMSG << "tcUrl = " << tc_url_ << endl;

                        ssize_t pos = -1;

                        for (int i = 0; i != 4; ++i)
                        {
                            pos = tc_url_.find("/", pos + 1);

                            if (i == 3 && pos != string::npos)
                            {
                                string stream_name = tc_url_.substr(pos + 1);
                                cout << LMSG << "stream_name:" << stream_name << endl;
                                SetStreamName(stream_name);

                                if (role_ == kClientPush || role_ == kPullServer)
                                {
                                    if (g_local_stream_center.RegisterStream(app_, stream_name_, this) == false)
                                    {
                                        cout << LMSG << "error" << endl;
                                        return kError;
                                    }
                                }
                            }

                            if (pos == string::npos)
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (! app_.empty())
        {
            String result("_result");
            Double transaction_id(trans_id);
            Map properties;

            String code("NetConnection.Connect.Success");
            Map information({{"code", (Any*)&code}});


            IoBuffer output;
            vector<Any*> connect_result = { 
                (Any*)&result, (Any*)&transaction_id, (Any*)&properties, (Any*)&information 
            };

            int ret = Amf0::Encode(connect_result, output);
            cout << LMSG << "Amf0 encode ret:" << ret << endl;
            if (ret == 0)
            {
                uint8_t* data = NULL;
                int len = output.Read(data, output.Size());

                if (data != NULL && len > 0)
                {
                    SetWindowAcknowledgementSize(0x10000000);
                    SetPeerBandwidth(0x10000000, 2);
                    SendUserControlMessage(0, 0);
                    SendRtmpMessage(2, 0, kAmf0Command, data, len);
                }
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnPlayCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command)
{
    SetClientPull();
    double trans_id = 0;

    if (amf_command.size() >= 4)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            cout << LMSG << "transaction_id:" << trans_id << endl;
        }

        if (stream_name_.empty())
        {
            if (amf_command[3]->GetString(stream_name_))
            {
                cout << LMSG << "stream_name:" << stream_name_ << endl;
            }
        }

        MediaPublisher* media_publisher = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_name_);

        if (media_publisher == NULL)
        {
            cout << LMSG << "no found app:" << app_ << ", stream_name_:" << stream_name_ << endl;
            return kClose;
        }

        String on_status("onStatus");
        Double transaction_id(0.0);
        Null null;

        String code("NetStream.Play.Start");
        Map information({{"code", (Any*)&code}});

        IoBuffer output;
        vector<Any*> play_result = {(Any*)&on_status, (Any*)&transaction_id, (Any*)&null, (Any*)&information};
        int ret = Amf0::Encode(play_result, output);
        cout << LMSG << "Amf0 encode ret:" << ret << endl;
        if (ret == 0)
        {
            uint8_t* data = NULL;
            int len = output.Read(data, output.Size());

            if (data != NULL && len > 0)
            {
                SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
            }

            SetMediaPublisher(media_publisher);
            media_publisher->AddRtmpPlayer(this);
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnPublishCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command)
{
    SetClientPush();

    double trans_id = 0;

    if (amf_command.size() >= 5)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            cout << LMSG << "transaction_id:" << trans_id << endl;
        }

        if (stream_name_.empty())
        {
            string stream_name;
            if (amf_command[3]->GetString(stream_name))
            {
                cout << LMSG << "stream_name:" << stream_name << endl;

                SetStreamName(stream_name);

                if (g_local_stream_center.RegisterStream(app_, stream_name_, this) == false)
                {
                    cout << LMSG << "error" << endl;
                    return kError;
                }
            }
        }
        else
        {
            if (g_local_stream_center.RegisterStream(app_, stream_name_, this) == false)
            {
                cout << LMSG << "app:" << app_ << ",stream:" << stream_name_ << " already register" << endl;
            }
        }

        String on_status("onStatus");
        Double transaction_id(0.0);
        Null null;

        String code("NetStream.Publish.Start");
        Map information({{"code", (Any*)&code}});

        IoBuffer output;
        vector<Any*> publish_result = {(Any*)&on_status, (Any*)&transaction_id, (Any*)&null, (Any*)&information};
        int ret = Amf0::Encode(publish_result, output);
        cout << LMSG << "Amf0 encode ret:" << ret << endl;
        if (ret == 0)
        {
            uint8_t* data = NULL;
            int len = output.Read(data, output.Size());

            if (data != NULL && len > 0)
            {
                SendUserControlMessage(0, 0);
                SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnCreateStreamCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command)
{
    double trans_id = 0;

    if (amf_command.size() >= 3)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            cout << LMSG << "transaction_id:" << trans_id << endl;

            String result("_result");
            Double transaction_id(trans_id);
            Null command_object;
            Double stream_id(1.0);

            IoBuffer output;
            vector<Any*> create_stream_result = { 
                (Any*)&result, (Any*)&transaction_id, (Any*)&command_object, (Any*)&stream_id 
            };

            int ret = Amf0::Encode(create_stream_result, output);
            cout << LMSG << "Amf0 encode ret:" << ret << endl;
            if (ret == 0)
            {
                uint8_t* data = NULL;
                int len = output.Read(data, output.Size());

                if (data != NULL && len > 0)
                {
                    SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
                }
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnResultCommand(AmfCommand& amf_command)
{
    double transaction_id = 0.0;
    if (amf_command.size() >= 2 && amf_command[1]->GetDouble(transaction_id))
    {
        cout << LMSG << "in _result, transaction_id:" << transaction_id << endl;
    }

    if (id_command_.count(transaction_id))
    {
        cout << LMSG << DumpIdCommand() << endl;
        string pre_call = id_command_[transaction_id];
        cout << LMSG << "pre_call " << transaction_id << " [" << pre_call << "]" << endl;
        if (pre_call == "connect")
        {
            SendReleaseStream();
            SendFCPublish();
            SendCreateStream();
            SendCheckBw();
        }
        else if (pre_call == "releaseStream")
        {
        }
        else if (pre_call == "FCPublish")
        {
            //SendCreateStream();
        }
        else if (pre_call == "createStream")
        {
            double stream_id = 1.0;
            if (amf_command.size() >= 4)
            {

                if (amf_command[3] != NULL && amf_command[3]->GetDouble(stream_id))
                {
                    cout << LMSG << "stream_id:" << stream_id << endl;
                }
            }

            SendPublish(stream_id);
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnStatusCommand(AmfCommand& amf_command)
{
    if (last_send_command_ == "publish")
    {
        if (! can_publish_)
        {
            can_publish_ = true;

            if (role_ == kPushServer)
            {
                media_publisher_->AddForwardRtmpServer(this);
            }
            else if (role_ == kClientPull)
            {
                media_publisher_->AddRtmpPlayer(this);
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnMetaData(RtmpMessage& rtmp_msg)
{
    string amf((const char*)rtmp_msg.msg, rtmp_msg.len);

    media_muxer_.OnMetaData(amf);

    AmfCommand amf_command;
    int ret = Amf0::Decode(amf,  amf_command);
    cout << LMSG << "ret:" << ret << ", amf_command.size():" <<  amf_command.size() << endl;

    for (size_t index = 0; index != amf_command.size(); ++index)
    {
        const auto& command = amf_command[index];

        if (command != NULL)
        {
            cout << LMSG << "v type:" << command->TypeStr() << endl;
        }
        else
        {
            cout << LMSG << "v NULL" << endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnVideoHeader(RtmpMessage& rtmp_msg)
{
    string video_header((const char*)rtmp_msg.msg + 5, rtmp_msg.len - 5);

    cout << LMSG << "recv video_header" << ",size:" << video_header.size() << endl;
    cout << Util::Bin2Hex(video_header) << endl;

    return media_muxer_.OnVideoHeader(video_header);
}

int RtmpProtocol::OnRtmpMessage(RtmpMessage& rtmp_msg)
{
    if (IsClientRole())
    {
        cout << LMSG << rtmp_msg.ToString() << endl;
    }

    switch (rtmp_msg.message_type_id)
    {
        case kSetChunkSize:
        {
            return OnSetChunkSize(rtmp_msg);
        }
        break;

        case kAcknowledgement:
        {
            return OnAcknowledgement(rtmp_msg);
        }
        break;

        case kWindowAcknowledgementSize:
        {
            return OnWindowAcknowledgementSize(rtmp_msg);
        }
        break;

        case kSetPeerBandwidth:
        {
            return OnSetPeerBandwidth(rtmp_msg);
        }
        break;

        case kUserControlMessage:
        {
            return OnUserControlMessage(rtmp_msg);
        }
        break;

        case kAudio:
        {
            return OnAudio(rtmp_msg);
        }
        break;

        case kVideo:
        {
            return OnVideo(rtmp_msg);
        }
        break;

        case kAmf0Command:
        {
            return OnAmf0Message(rtmp_msg);
        }
        break;

        case kMetaData:
        {
            return OnMetaData(rtmp_msg);
        }
        break;

        default: 
        {
            cout << LMSG << "message_type_id:" << (uint16_t)rtmp_msg.message_type_id << endl;
            return kError;
        }
        break;
    }

    cout << LMSG << "error" << endl;
    return kError;
}

int RtmpProtocol::OnStop()
{
    for (const auto& kv : csid_head_)
    {
        if (kv.second.msg != NULL)
        {
            free(kv.second.msg);
        }
    }

    cout << LMSG << "role:" << role_ << endl;

    csid_head_.clear();

    if (role_ == kClientPush)
    {
        for (auto& v : rtmp_forwards_)
        {
            v->OnStop();
        }

        g_local_stream_center.UnRegisterStream(app_, stream_name_, this);
    }
    else if (role_ == kPushServer)
    {
        if (media_publisher_ != NULL)
        {
            cout << LMSG << "remove forward" << endl;
            media_publisher_->RemoveForwardRtmpServer(this);
        }
    }
    else if (role_ == kClientPull)
    {
        if (media_publisher_ != NULL)
        {
            cout << LMSG << "remove player" << endl;
            media_publisher_->RemoveRtmpPlayer(this);
        }
    }
}

int RtmpProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    if (role_ == kClientPush || role_ == kPullServer)
    {
        cout << LMSG << "rtmp_forwards_.size():" << rtmp_forwards_.size() << ", rtmp_player_.size():" << rtmp_player_.size() << endl;
        media_muxer_.EveryNSecond(now_in_ms, interval, count);
    }
}

int RtmpProtocol::SendRtmpMessage(const uint32_t cs_id, const uint32_t& message_stream_id, const uint8_t& message_type_id, const uint8_t* data, const size_t& len)
{
    RtmpMessage rtmp_message;

    rtmp_message.cs_id = cs_id;
    rtmp_message.timestamp = 0;
    rtmp_message.timestamp_delta = 0;
    rtmp_message.message_length = len;
    rtmp_message.message_type_id = message_type_id;
    rtmp_message.message_stream_id = message_stream_id;

    rtmp_message.msg = (uint8_t*)data;
    rtmp_message.len = len;

    return SendData(rtmp_message);
}

int RtmpProtocol::SendData(const RtmpMessage& cur_info, const Payload& payload)
{
    const uint32_t cs_id = cur_info.cs_id;

    RtmpMessage& pre_info = csid_pre_info_[cs_id];

    uint32_t& pre_timestamp       = pre_info.timestamp;
    uint32_t& pre_timestamp_delta = pre_info.timestamp_delta;
    uint32_t& pre_message_length  = pre_info.message_length;
    uint8_t&  pre_message_type_id = pre_info.message_type_id;

    uint32_t cur_timestamp         = cur_info.timestamp;
    uint32_t cur_timestamp_delta   = cur_info.timestamp_delta;
    uint32_t cur_message_length    = cur_info.message_length;
    uint32_t cur_message_stream_id = cur_info.message_stream_id;
    uint8_t  cur_message_type_id   = cur_info.message_type_id;

    int chunk_count = 1;
    if (cur_message_length > out_chunk_size_)
    {
        chunk_count = cur_message_length / out_chunk_size_;

        if (cur_message_length % out_chunk_size_ != 0)
        {
            chunk_count += 1;
        }
    }

    int fmt = 0x0f;

    // new cs_id, fmt0
    if (pre_message_length == 0)
    {
        fmt = 0;
    }
    else
    {
        if (pre_message_length == cur_message_length)
        {
            if (pre_message_type_id == cur_message_type_id)
            {
                if (pre_timestamp_delta == cur_timestamp_delta)
                {
                    fmt = 3;
                }
                else
                {
                    fmt = 2;
                }
            }
            else
            {
                fmt = 1;
            }
        }
        else
        {
            fmt = 1;
        }
    }

    assert(fmt >= 0 && fmt <= 3);

    size_t data_pos = 0;
    for (int i = 0; i != chunk_count; ++i)
    {
        IoBuffer header;

        size_t send_len = cur_message_length - (i * out_chunk_size_);
        if (send_len > out_chunk_size_)
        {
            send_len = out_chunk_size_;
        }

        if (i == 0)
        {
            header.WriteU8(fmt << 6 | cs_id);

            if (fmt == 0)
            {
                header.WriteU24(cur_timestamp_delta);
                header.WriteU24(cur_message_length);
                header.WriteU8(cur_message_type_id);
                header.WriteU32(htobe32(cur_message_stream_id));
            }
            else if (fmt == 1)
            {
                header.WriteU24(cur_timestamp_delta);
                header.WriteU24(cur_message_length);
                header.WriteU8(cur_message_type_id);
            }
            else if (fmt == 2)
            {
                header.WriteU24(cur_timestamp_delta);
            }
            else if (fmt == 3)
            {
            }

            if (payload.IsVideo())
            {
				if (payload.IsIFrame())
    			{   
    			    cout << LMSG << "I frame" << endl;
    			    header.WriteU8(0x17);
    			}   
    			else
    			{   
    			    header.WriteU8(0x27);
    			}   

    			header.WriteU8(0x01); // AVC nalu

    			uint32_t compositio_time_offset = payload.GetPts32() - payload.GetDts32();

    			assert(compositio_time_offset >= 0); 

    			header.WriteU24(compositio_time_offset);
            }
        }
        else
        {
            fmt = 3;
            header.WriteU8(fmt << 6 | cs_id);
        }

        uint8_t* buf = NULL;
        int size = 0;

        size = header.Read(buf, header.Size());
        socket_->Send(buf, size);
        if (i == 0 && payload.IsVideo())
        {
            socket_->Send(cur_info.msg + data_pos, send_len - 5);
            data_pos += send_len - 5;
        }
        else
        {
            socket_->Send(cur_info.msg + data_pos, send_len);
            data_pos += send_len;
        }
    }

    csid_pre_info_[cs_id] = cur_info;

    // FIXME
    return 0;
}

int RtmpProtocol::SendMediaData(const Payload& payload)
{
    RtmpMessage rtmp_message;

    rtmp_message.cs_id = 6;

    if (payload.IsAudio())
    {
        rtmp_message.cs_id = 4;
    }

    rtmp_message.timestamp = payload.GetDts();

    if (csid_pre_info_.count(rtmp_message.cs_id) == 0)
    {
        rtmp_message.timestamp_delta = 0;
    }
    else
    {
        rtmp_message.timestamp_delta = rtmp_message.timestamp - csid_pre_info_[rtmp_message.cs_id].timestamp;
    }

    rtmp_message.message_length = payload.GetAllLen();

    if (payload.IsAudio())
    {
        rtmp_message.message_type_id = kAudio;
    }
    else if (payload.IsVideo())
    {
        rtmp_message.message_type_id = kVideo;
        rtmp_message.message_length += 5;
    }

    rtmp_message.msg = payload.GetAllData();
    rtmp_message.len = payload.GetAllLen();

    return SendData(rtmp_message, payload);
}

int RtmpProtocol::HandShakeStatus0()
{
    cout << LMSG << endl;

    uint8_t version = 3;

    socket_->Send(&version, 1);

    handshake_status_ = kStatus_1;
}

int RtmpProtocol::HandShakeStatus1()
{
    cout << LMSG << endl;

    IoBuffer io_buffer;

    uint32_t time = Util::GetNowMs();
    uint32_t zero = 0;

    uint8_t buf[1528];

    io_buffer.WriteU32(time);
    io_buffer.WriteU32(zero);
    io_buffer.Write(buf, sizeof(buf));

    uint8_t* data = NULL;

    io_buffer.Read(data, s1_len);

    socket_->Send(data, s1_len);

    handshake_status_ = kStatus_2;
}

int RtmpProtocol::SetOutChunkSize(const uint32_t& chunk_size)
{
    IoBuffer io_buffer;

    out_chunk_size_ = chunk_size;

    io_buffer.WriteU32(out_chunk_size_);

    uint8_t* data = NULL;

    io_buffer.Read(data, 4);

    SendRtmpMessage(2, 0, kSetChunkSize, data, 4);
}

int RtmpProtocol::SetWindowAcknowledgementSize(const uint32_t& ack_window_size)
{
    IoBuffer io_buffer;

    io_buffer.WriteU32(ack_window_size);

    uint8_t* data = NULL;

    io_buffer.Read(data, 4);

    SendRtmpMessage(2, 0, kWindowAcknowledgementSize, data, 4);
}

int RtmpProtocol::SetPeerBandwidth(const uint32_t& ack_window_size, const uint8_t& limit_type)
{
    IoBuffer io_buffer;

    io_buffer.WriteU32(ack_window_size);
    io_buffer.WriteU8(limit_type);

    uint8_t* data = NULL;

    io_buffer.Read(data, 5);

    SendRtmpMessage(2, 0, kSetPeerBandwidth, data, 5);
}

int RtmpProtocol::SendUserControlMessage(const uint16_t& event, const uint32_t& data)
{
    IoBuffer io_buffer;

    io_buffer.WriteU16(event);
    io_buffer.WriteU32(data);

    uint8_t* buf = NULL;

    io_buffer.Read(buf, 6);

    SendRtmpMessage(2, 0, kUserControlMessage, buf, 6);
}

int RtmpProtocol::SendConnect(const string& url)
{
    cout << LMSG << "url:" << url << endl;
    RtmpUrl rtmp_url;
    ParseRtmpUrl(url, rtmp_url);

    stream_name_ = rtmp_url.stream_name;

    String command_name("connect");
    Double transaction_id(GetTransactionId());

    String app(rtmp_url.app);
    //String tc_url("rtmp://" + rtmp_url.ip + ":" + Util::Num2Str(rtmp_url.port) + "/" + rtmp_url.app);
    String tc_url("rtmp://" + rtmp_url.ip + "/" + rtmp_url.app);
    
    map<string, Any*> m = {{"app", &app}, {"tcUrl", &tc_url}};

    map<string, Any*> empty;

    Map command_object(m);
    Map optional_uer_args(empty);

    vector<Any*> connect = {(Any*)&command_name, (Any*)&transaction_id, (Any*)&command_object, (Any*)&optional_uer_args};

    IoBuffer output;

    int ret = Amf0::Encode(connect, output);
    cout << LMSG << "Amf0 encode ret:" << ret << endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "connect";

            id_command_[transaction_id_] = last_send_command_;

            cout << LMSG << "send [" << last_send_command_ << " command]" << endl;
        }
    }
}

int RtmpProtocol::SendCreateStream()
{
    String command_name("createStream");
    Double transaction_id(GetTransactionId());
    Null null;

    vector<Any*> create_stream = {&command_name, &transaction_id, &null};

    IoBuffer output;

    int ret = Amf0::Encode(create_stream, output);
    cout << LMSG << "Amf0 encode ret:" << ret << endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "createStream";
            id_command_[transaction_id_] = last_send_command_;
            cout << LMSG << "send [" << last_send_command_ << " command]" << endl;
        }
    }
}

int RtmpProtocol::SendReleaseStream()
{
    String command_name("releaseStream");
    Double transaction_id(GetTransactionId());
    Null null;
    String playpath(stream_name_);

    vector<Any*> releaseStream = {&command_name, &transaction_id, &null, &playpath};

    IoBuffer output;

    int ret = Amf0::Encode(releaseStream, output);
    cout << LMSG << "Amf0 encode ret:" << ret << endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "releaseStream";
            id_command_[transaction_id_] = last_send_command_;
            cout << LMSG << "send [" << last_send_command_ << " command]" << endl;
        }
    }
}

int RtmpProtocol::SendFCPublish()
{
    String command_name("FCPublish");
    Double transaction_id(GetTransactionId());
    Null null;
    String playpath("");

    vector<Any*> fcpublish = {&command_name, &transaction_id, &null, &playpath};

    IoBuffer output;

    int ret = Amf0::Encode(fcpublish, output);
    cout << LMSG << "Amf0 encode ret:" << ret << endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(8, 1, kAmf0Command, data, len);

            last_send_command_ = "FCPublish";
            id_command_[transaction_id_] = last_send_command_;
            cout << LMSG << "send [" << last_send_command_ << " command]" << endl;
        }
    }
}

int RtmpProtocol::SendCheckBw()
{
    String command_name("_checkbw");
    Double transaction_id(GetTransactionId());
    Null null;

    vector<Any*> checkbw = {&command_name, &transaction_id, &null};

    IoBuffer output;

    int ret = Amf0::Encode(checkbw, output);
    cout << LMSG << "Amf0 encode ret:" << ret << endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "_checkbw";
            id_command_[transaction_id_] = last_send_command_;
            cout << LMSG << "send [" << last_send_command_ << " command]" << endl;
        }
    }
}

int RtmpProtocol::SendPublish(const double& stream_id)
{
    String command_name("publish");
    Double transaction_id(GetTransactionId());
    Null null;
    String stream_name(stream_name_);
    String publish_type("live");

    vector<Any*> publish = {&command_name, &transaction_id, &null, &stream_name, &publish_type};

    IoBuffer output;

    int ret = Amf0::Encode(publish, output);
    cout << LMSG << "Amf0 encode ret:" << ret << endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(8, 1, kAmf0Command, data, len);
            last_send_command_ = "publish";
            id_command_[transaction_id_] = last_send_command_;
            cout << LMSG << "send [" << last_send_command_ << " command]" << endl;
        }
    }
}

int RtmpProtocol::SendAudio(const RtmpMessage& audio)
{
}

int RtmpProtocol::SendVideo(const RtmpMessage& video)
{
}

int RtmpProtocol::ConnectForwardRtmpServer(const string& ip, const uint16_t& port)
{
    int fd = CreateNonBlockTcpSocket();

    if (fd < 0)
    {
        cout << LMSG << "ConnectForwardRtmpServer ret:" << fd << endl;
        return -1;
    }

    int ret = ConnectHost(fd, ip, port);

    if (ret < 0 && errno != EINPROGRESS)
    {
        cout << LMSG << "Connect ret:" << ret << endl;
        return -1;
    }

    Fd* socket = new TcpSocket(epoller_, fd, (SocketHandle*)rtmp_mgr_);

    RtmpProtocol* rtmp_forward = rtmp_mgr_->GetOrCreateProtocol(*socket);

    rtmp_forward->SetApp(app_);
    rtmp_forward->SetStreamName(stream_name_);
    rtmp_forward->SetPushServer();

    if (errno == EINPROGRESS)
    {
        rtmp_forward->GetTcpSocket()->SetConnecting();
        rtmp_forward->GetTcpSocket()->EnableWrite();
    }
    else
    {
        rtmp_forward->GetTcpSocket()->SetConnected();
        rtmp_forward->GetTcpSocket()->EnableRead();

        rtmp_forward->HandShakeStatus0();
        rtmp_forward->HandShakeStatus1();
    }

    rtmp_forward->SetMediaPublisher(this);

    cout << LMSG << endl;
}

int RtmpProtocol::ConnectFollowServer(const string& ip, const uint16_t& port)
{
    int fd = CreateNonBlockTcpSocket();

    if (fd < 0)
    {
        cout << LMSG << "ConnectFollowServer ret:" << fd << endl;
        return -1;
    }

    int ret = ConnectHost(fd, ip, port);

    if (ret < 0 && errno != EINPROGRESS)
    {
        cout << LMSG << "Connect ret:" << ret << endl;
        return -1;
    }

    Fd* socket = new TcpSocket(epoller_, fd, (SocketHandle*)server_mgr_);

    ServerProtocol* server_dst = server_mgr_->GetOrCreateProtocol(*socket);

    server_dst->SetPushServer();

    server_dst->SetMediaPublisher(this);
    server_dst->SetApp(app_);
    server_dst->SetStreamName(stream_name_);

    if (errno == EINPROGRESS)
    {
        server_dst->GetTcpSocket()->SetConnecting();
        server_dst->GetTcpSocket()->EnableWrite();
    }
    else
    {
        server_dst->GetTcpSocket()->SetConnected();
        server_dst->GetTcpSocket()->EnableRead();
    }

    cout << LMSG << endl;
}

int RtmpProtocol::OnConnected()
{
    cout << LMSG << endl;

    GetTcpSocket()->SetConnected();
    GetTcpSocket()->EnableRead();
    GetTcpSocket()->DisableWrite();

    if (role_ == kPushServer)
    {
        HandShakeStatus0();
        HandShakeStatus1();
    }
}
