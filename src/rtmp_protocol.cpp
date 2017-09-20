#include <iostream>

#include "amf_0.h"
#include "any.h"
#include "assert.h"
#include "bit_buffer.h"
#include "common_define.h"
#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "fd.h"
#include "stream_mgr.h"
#include "tcp_socket.h"
#include "util.h"

using namespace std;
using namespace socket_util;

using any::Any;
using any::Int;
using any::Double;
using any::String;
using any::Vector;
using any::Map;
using any::Null;

static uint32_t s0_len = 1;
static uint32_t s1_len = 4/*time*/ + 4/*zero*/ + 1528/*random*/;
static uint32_t s2_len = 4/*time*/ + 4/*time2*/ + 1528/*random*/;


RtmpProtocol::RtmpProtocol(Epoller* epoller, Fd* fd, StreamMgr* stream_mgr)
    :
    epoller_(epoller),
    socket_(fd),
    stream_mgr_(stream_mgr),
    handshake_status_(kStatus_0),
    role_(kUnknownRole),
    in_chunk_size_(128),
    out_chunk_size_(128),
    video_fps_(0),
    audio_fps_(0),
    video_frame_recv_(0),
    audio_frame_recv_(0),
    last_key_video_frame_(0),
    last_key_audio_frame_(0),
    last_calc_fps_ms_(0),
    last_calc_video_frame_(0),
    last_calc_audio_frame_(0),
    rtmp_src_(NULL),
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

#ifdef DEBUG
            cout << LMSG << "fmt:" << (uint16_t)fmt << ",cs_id:" << cs_id << ",io_buffer.Size():" << io_buffer.Size() << endl;
#endif

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

#ifdef DEBUG
                    cout << LMSG << "fmt_0|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
#endif
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

#ifdef DEBUG
                    cout << LMSG << "fmt_1|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
#endif
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

#ifdef DEBUG
                    cout << LMSG << "fmt_2|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
#endif
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 3)
            {
                RtmpMessage& rtmp_msg = csid_head_[cs_id];

#ifdef DEBUG
                cout << LMSG << "fmt_3|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
#endif
            }

            if (io_buffer.Size() >= chunk_header_len + message_header_len)
            {
                if (rtmp_msg.len == 0)
                {
                    rtmp_msg.msg = (uint8_t*)malloc(rtmp_msg.message_length);
                }

                uint32_t read_len = rtmp_msg.message_length - rtmp_msg.len;
                if (read_len > in_chunk_size_)
                {
                    read_len = in_chunk_size_;
                }

                if (io_buffer.Size() >= chunk_header_len + message_header_len + read_len)
                {
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
#ifdef DEBUG
                        cout << LMSG << "enough chunk data, no enough messge data,message_length:" << rtmp_msg.message_length 
                                     << ",cur_len:" << rtmp_msg.len << ",io_buffer.Size():" << io_buffer.Size() << endl;
#endif

                        // 这里也要返回success
                        return kSuccess;
                    }
                }
                else
                {
#ifdef DEBUG
                    cout << LMSG << "no enough chunk data, io_buffer.Size():" << io_buffer.Size() << endl;
#endif
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

#ifdef DEBUG
            cout << LMSG << "message done|typeid:" << (uint16_t)rtmp_msg.message_type_id << endl;
            cout << LMSG << ",audio_queue_.size():" << audio_queue_.size()
                         << ",video_queue_.size():" << video_queue_.size()
                         << endl;
#endif

            OnRtmpMessage(rtmp_msg);

            if (rtmp_msg.message_type_id == 8)
            {
            }
            else if (rtmp_msg.message_type_id == 9)
            {
            }
            else
            {
                free(rtmp_msg.msg);
            }

            rtmp_msg.msg = NULL;
            rtmp_msg.len = 0;

            return kSuccess;
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
                    SendConnect("rtmp://127.0.0.1:1936/test/john");
                    SendCreateStream();

                    return kSuccess;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else
            {
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
    return kError;
}


int RtmpProtocol::OnRtmpMessage(RtmpMessage& rtmp_msg)
{
    //cout << LMSG << rtmp_msg.ToString() << endl;
    switch (rtmp_msg.message_type_id)
    {
        case kSetChunkSize:
        {
            BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

            uint32_t chunk_size = 0;
            bit_buffer.GetBytes(4, chunk_size);

            cout << LMSG << "chunk_size:" << in_chunk_size_ << "->" << chunk_size << endl;

            in_chunk_size_ = chunk_size;
        }
        break;

        case kWindowAcknowledgementSize:
        {
            BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

            uint32_t ack_window_size = 0;
            bit_buffer.GetBytes(4, ack_window_size);

            cout << LMSG << "ack_window_size:" << ack_window_size << endl;

            SendUserControlMessage(0, 0);
        }
        break;

        case kAudio:
        {
            bool push = false;

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

                if (audio_frame_recv_ <= 10)
                {
                    cout << "audio_frame_recv_:" << audio_frame_recv_
                         << ",sound_format:" << (int)sound_format
                         << ",sound_rate:" << (int)sound_rate
                         << ",sound_size:" << (int)sound_size
                         << ",sound_type:" << (int)sound_type
                         << ",aac_packet_type:" << (int)aac_packet_type
                         << endl;
                }

                if (sound_format == 10 && aac_packet_type == 0)
                {
                    aac_header_.assign((const char*)rtmp_msg.msg, rtmp_msg.len);
                    cout << LMSG << "recv aac_header_" << ",size:" << aac_header_.size() << endl;
                    cout << Util::Bin2Hex(rtmp_msg.msg, rtmp_msg.len) << endl;
                }
                else
                {
                    push = true;
                }
            }
            else
            {
                cout << LMSG << "impossible?" << endl;
            }

            if (push)
            {
                Payload audio_payload(rtmp_msg.msg, rtmp_msg.len);

                audio_queue_.insert(make_pair(audio_frame_recv_, audio_payload));

                // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
                if ((audio_fps_ != 0 && audio_queue_.size() > 10 * audio_fps_) || audio_queue_.size() >= 800)
                {
                    audio_queue_.erase(audio_queue_.begin());
                }
            }

            ++audio_frame_recv_;

            for (auto& dst : rtmp_forwards_)
            {
                if (dst->CanPublish())
                {
                    dst->SendMediaData(rtmp_msg);
                }
            }

            for (auto& player : rtmp_player_)
            {
                player->SendMediaData(rtmp_msg);
                //cout << LMSG << "send audio to player:" << player << endl;
            }
        }

        break;

        case kVideo:
        {
            bool push = false;

            uint8_t frame_type = 0xff;
            uint8_t codec_id = 0xff;
            uint8_t avc_packet_type = 0xff;

            if (rtmp_msg.len >= 2)
            {
                BitBuffer bit_buffer(rtmp_msg.msg, 2);

                bit_buffer.GetBits(4, frame_type);
                bit_buffer.GetBits(4, codec_id);
                bit_buffer.GetBits(8, avc_packet_type);

                if (video_frame_recv_ <= 10)
                {
                    cout << "video_frame_recv_:" << video_frame_recv_
                         << ",frame_type:" << (int)frame_type 
                         << ",codec_id:" << (int)codec_id
                         << ",avc_packet_type:" << (int)avc_packet_type
                         << endl;
                }

                if (codec_id == 7 && avc_packet_type == 0)
                {
                    avc_header_.assign((const char*)rtmp_msg.msg, rtmp_msg.len);
                    cout << LMSG << "recv avc_header_" << ",size:" << avc_header_.size() << endl;
                    cout << Util::Bin2Hex(rtmp_msg.msg, rtmp_msg.len) << endl;
                }
                else
                {
                    push = true;
                }
            }

            if (push)
            {
                Payload video_payload(rtmp_msg.msg, rtmp_msg.len);

                video_queue_.insert(make_pair(video_frame_recv_, video_payload));
                //
                // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
                if ((video_fps_ != 0 && video_queue_.size() > 10 * video_fps_) || video_queue_.size() >= 800)
                {
                    video_queue_.erase(video_queue_.begin());
                }

                // key frame
                if (frame_type == 1)
                {
                    cout << LMSG << "video key frame:" << video_frame_recv_ << "," << audio_frame_recv_ << endl;
                    last_key_video_frame_ = video_frame_recv_;
                    last_key_audio_frame_ = audio_frame_recv_;
                }

                ++video_frame_recv_;
            }

            for (auto& dst : rtmp_forwards_)
            {
                if (dst->CanPublish())
                {
                    dst->SendMediaData(rtmp_msg);
                }
            }

            for (auto& player : rtmp_player_)
            {
                //cout << LMSG << "send video to player:" << player << endl;
                player->SendMediaData(rtmp_msg);
            }
        }
        break;

        case kAmf0Command:
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
                    map<string, Any*> command_object;

                    cout << LMSG << "[" << command << " msg]" << endl;
                    if (command == "connect")
                    {
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
                                        if (kv.second->GetString(app_))
                                        {
                                            auto pos = app_.find("/");
                                            if (pos != string::npos)
                                            {
                                                app_ = app_.substr(0, pos);
                                            }
                                            cout << LMSG << "app = " << app_ << endl;
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
                                                    stream_name_ = tc_url_.substr(pos + 1);
                                                    cout << "stream_name_:" << stream_name_ << endl;
                                                    stream_mgr_->RegisterStream(app_, stream_name_, this);
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

                                ret = Amf0::Encode(connect_result, output);
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
                                        SendRtmpMessage(kAmf0Command, data, len);
                                    }
                                }
                            }
                        }
                    }
                    else if (command == "play")
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

                            String on_status("onStatus");
                            Double transaction_id(0.0);
                            Null null;

                            String code("NetStream.Play.Start");
                            Map information({{"code", (Any*)&code}});

                            IoBuffer output;
                            vector<Any*> play_result = {(Any*)&on_status, (Any*)&transaction_id, (Any*)&null, (Any*)&information};
                            ret = Amf0::Encode(play_result, output);
                            cout << LMSG << "Amf0 encode ret:" << ret << endl;
                            if (ret == 0)
                            {
                                RtmpProtocol* rtmp_publisher = stream_mgr_->GetRtmpProtocolByAppStream(app_, stream_name_);
                                if (rtmp_publisher == NULL)
                                {
                                }
                                else
                                {
                                    SetRtmpSrc(rtmp_publisher);
                                    rtmp_publisher->AddRtmpPlayer(this);
                                }

                                uint8_t* data = NULL;
                                int len = output.Read(data, output.Size());

                                if (data != NULL && len > 0)
                                {
                                    SendRtmpMessage(kAmf0Command, data, len);
                                }
                            }
                        }
                    }
                    else if (command == "publish")
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
                                if (amf_command[3]->GetString(stream_name_))
                                {
                                    cout << LMSG << "stream_name:" << stream_name_ << endl;
                                    stream_mgr_->RegisterStream(app_, stream_name_, this);
                                }
                            }

                            String on_status("onStatus");
                            Double transaction_id(0.0);
                            Null null;

                            String code("NetStream.Publish.Start");
                            Map information({{"code", (Any*)&code}});

                            IoBuffer output;
                            vector<Any*> publish_result = {(Any*)&on_status, (Any*)&transaction_id, (Any*)&null, (Any*)&information};
                            ret = Amf0::Encode(publish_result, output);
                            cout << LMSG << "Amf0 encode ret:" << ret << endl;
                            if (ret == 0)
                            {
                                uint8_t* data = NULL;
                                int len = output.Read(data, output.Size());

                                if (data != NULL && len > 0)
                                {
                                    SendUserControlMessage(0, 0);
                                    SendRtmpMessage(kAmf0Command, data, len);
                                }
                            }
                        }
                    }
                    else if (command == "createStream")
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

                                ret = Amf0::Encode(create_stream_result, output);
                                cout << LMSG << "Amf0 encode ret:" << ret << endl;
                                if (ret == 0)
                                {
                                    uint8_t* data = NULL;
                                    int len = output.Read(data, output.Size());

                                    if (data != NULL && len > 0)
                                    {
                                        SendRtmpMessage(kAmf0Command, data, len);
                                    }
                                }
                            }
                        }
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
                    else if (command == "_result")
                    {
                        if (last_send_command_ == "createStream")
                        {
                            SendPublish();
                        }
                    }
                    else if (command == "_error")
                    {
                    }
                    else if (command == "onStatus")
                    {
                        if (last_send_command_ == "publish")
                        {
                            can_publish_ = true;

                            rtmp_src_->OnNewRtmpPlayer(this);
                        }
                    }
                }
            }
        }
        break;

        case kMetaData:
        {
            ConnectForwardServer("127.0.0.1", 1936);

            string amf((const char*)rtmp_msg.msg, rtmp_msg.len);

            metadata_ = amf;

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
        }
        break;

        default: 
        {
        }
        break;

    }
}

int RtmpProtocol::OnNewRtmpPlayer(RtmpProtocol* protocol)
{
    cout << LMSG << endl;

    SendRtmpMessage(kMetaData, (const uint8_t*)metadata_.data(), metadata_.size());
    protocol->SendRtmpMessage(kAudio, (const uint8_t*)aac_header_.data(), aac_header_.size());
    protocol->SendRtmpMessage(kVideo, (const uint8_t*)avc_header_.data(), avc_header_.size());

    auto iter_key_video = video_queue_.find(last_key_video_frame_);

    if (iter_key_video != video_queue_.end())
    {
        for (auto it = iter_key_video; it != video_queue_.end(); ++it)
        {
            RtmpMessage video;

            video.message_length = it->second.GetLen();
            video.msg = it->second.GetPtr();
            video.len = it->second.GetLen();
            video.message_type_id = kVideo;
            video.timestamp_calc = it->second.GetTimestamp();

            cout << LMSG << "send video " << it->first << ",len:" << video.len << ",timestamp_calc:" << video.timestamp_calc << endl;

            protocol->SendMediaData(video);
        }

        auto iter_key_audio = audio_queue_.find(last_key_audio_frame_);

        for (auto it = iter_key_audio; it != audio_queue_.end(); ++it)
        {
            RtmpMessage audio;

            audio.message_length = it->second.GetLen();
            audio.msg = it->second.GetPtr();
            audio.len = it->second.GetLen();
            audio.message_type_id = kAudio;
            audio.timestamp_calc = it->second.GetTimestamp();

            protocol->SendMediaData(audio);

            cout << LMSG << "send audio " << it->first << ",len:" << audio.len << ",timestamp_calc:" << audio.timestamp_calc << endl;
        }
    }
}

int RtmpProtocol::OnStop()
{
    audio_queue_.clear();
    video_queue_.clear();

    for (const auto& kv : csid_head_)
    {
        if (kv.second.msg != NULL)
        {
            free(kv.second.msg);
        }
    }

    csid_head_.clear();

    if (role_ == kClientPush)
    {
        for (auto& v : rtmp_forwards_)
        {
            v->OnStop();
        }
    }
    else if (role_ == kPushServer)
    {
        cout << LMSG << "remove forward" << endl;
        rtmp_src_->RemoveForward(this);
    }
    else if (role_ == kClientPull)
    {
        cout << LMSG << "remove player" << endl;
        rtmp_src_->RemovePlayer(this);
    }
}

int RtmpProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    if (last_calc_fps_ms_ == 0)
    {
        last_calc_fps_ms_ = now_in_ms;
        last_calc_video_frame_ = video_frame_recv_;
        last_calc_audio_frame_ = audio_frame_recv_;
    }
    else
    {
        video_fps_ = video_frame_recv_ - last_calc_video_frame_;
        audio_fps_ = audio_frame_recv_ - last_calc_audio_frame_;

        //cout << LMSG << "[STAT] stream:" << stream_name_ << ",video_fps:" << video_fps_ << ",audio_fps:" << audio_fps_ << ",interval:" << interval << endl;

        last_calc_fps_ms_ = now_in_ms;
        last_calc_video_frame_ = video_frame_recv_;
        last_calc_audio_frame_ = audio_frame_recv_;
    }
}

int RtmpProtocol::SendRtmpMessage(const uint8_t& message_type_id, const uint8_t* data, const size_t& len)
{
    cout << LMSG << "message_type_id:" << (uint16_t)message_type_id << ", message_length:" << len << endl;
    uint32_t cs_id = 2;
    uint8_t  fmt = 0;

    IoBuffer chunk_header;
    IoBuffer message_header;

    uint32_t timestamp = Util::GetNowMs();

    if (message_type_id == kVideo || message_type_id == kAudio || message_type_id == kMetaData)
    {
        timestamp = 0;
    }

    if (len <= out_chunk_size_)
    {
        chunk_header.WriteU8(fmt << 6 | cs_id);

        message_header.WriteU24(timestamp);
        message_header.WriteU24(len);
        message_header.WriteU8(message_type_id);
        message_header.WriteU32(0);

        uint8_t* buf = NULL;
        int size = 0;

        size = chunk_header.Read(buf, chunk_header.Size());

        socket_->Send(buf, size);

        size = message_header.Read(buf, message_header.Size());
        cout << Util::Bin2Hex(buf, size) << endl;
        cout << Util::Bin2Hex(data, len) << endl;

        socket_->Send(buf, size);
        socket_->Send(data, len);
    }
}

int RtmpProtocol::SendMediaData(const RtmpMessage& media)
{
    int chunk_count = 1;
    if (media.len > out_chunk_size_)
    {
        chunk_count = media.len / out_chunk_size_;

        if (media.len % out_chunk_size_ != 0)
        {
            chunk_count += 1;
        }
    }

#ifdef DEBUG
    cout << LMSG << "chunk_count:" << chunk_count << ",message_length:" << media.message_length << endl;
#endif

    bool is_video = false;
    uint8_t fmt = 0;
    uint32_t cs_id = 4;
    if (media.message_type_id == kVideo)
    {
        is_video = true;
    }

    uint32_t timestamp_delta = 0;
    size_t data_pos = 0;

    //cout << media.ToString() << endl;

    if (is_video)
    {
        ++video_frame_send_;

        timestamp_delta = media.timestamp_calc - last_video_timestamp_;


        if (last_video_timestamp_ == 0)
        {
            last_video_timestamp_ = media.timestamp_calc;
            last_video_message_length_ = media.message_length;
            last_message_type_id_ = media.message_type_id;

            for (int i = 0; i != chunk_count; ++i)
            {
                size_t send_len = media.len - (i * out_chunk_size_);
                if (send_len > out_chunk_size_)
                {
                    send_len = out_chunk_size_;
                }

#ifdef DEBUG
                cout << LMSG << "send_len:" << send_len << endl;
#endif

                if (i == 0)
                {
                    fmt = 0;
                }
                else
                {
                    fmt = 3;
                }

                IoBuffer header;

                header.WriteU8(fmt << 6 | cs_id);

                if (fmt == 0)
                {
                    header.WriteU24(media.timestamp_calc);
                    header.WriteU24(media.message_length);
                    header.WriteU8(media.message_type_id);
                    header.WriteU32(0x01000000);
                }

                uint8_t* buf = NULL;
                int size = 0;


                size = header.Read(buf, header.Size());
                socket_->Send(buf, size);

#ifdef DEBUG
                cout << Util::Bin2Hex(buf, size) << endl;
#endif

                socket_->Send(media.msg + data_pos, send_len);
                data_pos += send_len;
            }
        }
        else
        {
            if (media.message_length == last_video_message_length_ && media.message_type_id == last_message_type_id_)
            {
                fmt = 2;
            }
            else if (media.message_length == last_video_message_length_ && media.message_type_id == last_message_type_id_ && last_video_timestamp_delta_ == timestamp_delta)
            {
                fmt = 3;
            }
            else
            {
                fmt = 1;
            }

            last_video_message_length_ = media.message_length;
            last_message_type_id_ = media.message_type_id;
            last_video_timestamp_ = media.timestamp_calc;
            last_video_timestamp_delta_ = timestamp_delta;

            for (int i = 0; i != chunk_count; ++i)
            {
                size_t send_len = media.len - (i * out_chunk_size_);
                if (send_len > out_chunk_size_)
                {
                    send_len = out_chunk_size_;
                }

#ifdef DEBUG
                cout << LMSG << "send_len:" << send_len << endl;
#endif

                if (i != 0)
                {
                    fmt = 3;
                }

                IoBuffer header;

                header.WriteU8(fmt << 6 | cs_id);

                if (fmt == 1)
                {
                    header.WriteU24(timestamp_delta);
                    header.WriteU24(media.message_length);
                    header.WriteU8(media.message_type_id);
                }
                else if (fmt == 2)
                {
                    header.WriteU24(timestamp_delta);
                }

                uint8_t* buf = NULL;
                int size = 0;

                size = header.Read(buf, header.Size());
                socket_->Send(buf, size);
                socket_->Send(media.msg + data_pos, send_len);
                data_pos += send_len;

#ifdef DEBUG
                cout << Util::Bin2Hex(buf, size) << endl;
#endif
            }
        }
    }
    else
    {
        ++audio_frame_send_;

        cs_id = 5;

        timestamp_delta = media.timestamp_calc - last_audio_timestamp_;

        if (last_audio_timestamp_ == 0)
        {
            last_audio_timestamp_ = media.timestamp_calc;
            last_audio_message_length_ = media.message_length;
            last_message_type_id_ = media.message_type_id;

            for (int i = 0; i != chunk_count; ++i)
            {
                size_t send_len = media.len - (i * out_chunk_size_);
                if (send_len > out_chunk_size_)
                {
                    send_len = out_chunk_size_;
                }

#ifdef DEBUG
                cout << LMSG << "send_len:" << send_len << endl;
#endif

                if (i == 0)
                {
                    fmt = 0;
                }
                else
                {
                    fmt = 3;
                }

                IoBuffer header;

                header.WriteU8(fmt << 6 | cs_id);

                if (fmt == 0)
                {
                    header.WriteU24(media.timestamp_calc);
                    header.WriteU24(media.message_length);
                    header.WriteU8(media.message_type_id);
                    header.WriteU32(1);
                }

                uint8_t* buf = NULL;
                int size = 0;

                size = header.Read(buf, header.Size());
                socket_->Send(buf, size);

                socket_->Send(media.msg + data_pos, send_len);
                data_pos += send_len;
            }
        }
        else
        {
            if (media.message_length == last_audio_message_length_ && media.message_type_id == last_message_type_id_)
            {
                fmt = 2;
            }
            else if (media.message_length == last_audio_message_length_ && media.message_type_id == last_message_type_id_ && last_audio_timestamp_delta_ == timestamp_delta)
            {
                fmt = 3;
            }
            else
            {
                fmt = 1;
            }

            last_audio_message_length_ = media.message_length;
            last_message_type_id_ = media.message_type_id;
            last_audio_timestamp_ = media.timestamp_calc;
            last_audio_timestamp_delta_ = timestamp_delta;

            for (int i = 0; i != chunk_count; ++i)
            {
                size_t send_len = media.len - (i * out_chunk_size_);
                if (send_len > out_chunk_size_)
                {
                    send_len = out_chunk_size_;
                }

#ifdef DEBUG
                cout << LMSG << "send_len:" << send_len << endl;
#endif

                if (i != 0)
                {
                    fmt = 3;
                }

                IoBuffer header;

                header.WriteU8(fmt << 6 | cs_id);

                if (fmt == 1)
                {
                    header.WriteU24(timestamp_delta);
                    header.WriteU24(media.message_length);
                    header.WriteU8(media.message_type_id);
                }
                else if (fmt == 2)
                {
                    header.WriteU24(timestamp_delta);
                }

                uint8_t* buf = NULL;
                int size = 0;

                size = header.Read(buf, header.Size());
                socket_->Send(buf, size);
                socket_->Send(media.msg + data_pos, send_len);
                data_pos += send_len;
            }
        }
    }
}

int RtmpProtocol::HandShakeStatus0()
{
    uint8_t version = 1;

    socket_->Send(&version, 1);

    handshake_status_ = kStatus_1;
}

int RtmpProtocol::HandShakeStatus1()
{
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

    SendRtmpMessage(kSetChunkSize, data, 4);
}

int RtmpProtocol::SetWindowAcknowledgementSize(const uint32_t& ack_window_size)
{
    IoBuffer io_buffer;

    io_buffer.WriteU32(ack_window_size);

    uint8_t* data = NULL;

    io_buffer.Read(data, 4);

    SendRtmpMessage(kWindowAcknowledgementSize, data, 4);
}

int RtmpProtocol::SetPeerBandwidth(const uint32_t& ack_window_size, const uint8_t& limit_type)
{
    IoBuffer io_buffer;

    io_buffer.WriteU32(ack_window_size);
    io_buffer.WriteU8(limit_type);

    uint8_t* data = NULL;

    io_buffer.Read(data, 5);

    SendRtmpMessage(kSetPeerBandwidth, data, 5);
}

int RtmpProtocol::SendUserControlMessage(const uint16_t& event, const uint32_t& data)
{
    IoBuffer io_buffer;

    io_buffer.WriteU16(event);
    io_buffer.WriteU32(data);

    uint8_t* buf = NULL;

    io_buffer.Read(buf, 6);

    SendRtmpMessage(kUserControlMessage, buf, 6);
}

int RtmpProtocol::SendConnect(const string& url)
{
    RtmpUrl rtmp_url;
    ParseRtmpUrl(url, rtmp_url);

    stream_name_ = rtmp_url.stream_name;

    String command_name("connnect");
    Double transaction_id(1.0);

    String app(rtmp_url.app);
    String tc_url("rtmp://" + rtmp_url.ip + ":" + Util::Num2Str(rtmp_url.port) + "/" + rtmp_url.app);
    
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
            SendRtmpMessage(kAmf0Command, data, len);

            last_send_command_ = "connect";
        }
    }
}

int RtmpProtocol::SendCreateStream()
{
    String command_name("createStream");
    Double transaction_id(1.0);
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
            SendRtmpMessage(kAmf0Command, data, len);

            last_send_command_ = "createStream";
        }
    }
}

int RtmpProtocol::SendPublish()
{
    String command_name("publish");
    Double transaction_id(0.0);
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
            SendRtmpMessage(kAmf0Command, data, len);
            last_send_command_ = "publish";
        }
    }
}

int RtmpProtocol::SendAudio(const RtmpMessage& audio)
{
}

int RtmpProtocol::SendVideo(const RtmpMessage& video)
{
}

int RtmpProtocol::ConnectForwardServer(const string& ip, const uint16_t& port)
{
    int fd = CreateNonBlockTcpSocket();

    if (fd < 0)
    {
        cout << LMSG << "ConnectForwardServer ret:" << fd << endl;
        return -1;
    }

    int ret = Connect(fd, ip, port);

    if (ret < 0 && errno != EINPROGRESS)
    {
        cout << LMSG << "Connect ret:" << ret << endl;
        return -1;
    }

    Fd* socket = new TcpSocket(epoller_, fd, (SocketHandle*)stream_mgr_);

    RtmpProtocol* rtmp_dst = stream_mgr_->GetOrCreateProtocol(*socket);

    rtmp_dst->SetPushServer();

    if (errno == EINPROGRESS)
    {
        rtmp_dst->GetTcpSocket()->SetConnecting();
        rtmp_dst->GetTcpSocket()->EnableWrite();
    }
    else
    {
        rtmp_dst->GetTcpSocket()->SetConnected();
        rtmp_dst->GetTcpSocket()->EnableRead();

        rtmp_dst->HandShakeStatus0();
        rtmp_dst->HandShakeStatus1();
    }

    rtmp_dst->SetRtmpSrc(this);

    rtmp_forwards_.insert(rtmp_dst);

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
