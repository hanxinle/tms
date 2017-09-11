#include <iostream>

#include "amf_0.h"
#include "any.h"
#include "assert.h"
#include "bit_buffer.h"
#include "common_define.h"
#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "fd.h"
#include "util.h"

using namespace std;

using any::Any;
using any::Int;
using any::Double;
using any::String;
using any::Vector;
using any::Map;
using any::Null;

RtmpProtocol::RtmpProtocol(Fd* fd)
    :
    socket_(fd),
    handshake_status_(kStatus_0),
    in_chunk_size_(128),
    out_chunk_size_(128),
    video_fps_(0),
    audio_fps_(0),
    video_frame_recv_(0),
    audio_frame_recv_(0),
    last_calc_fps_ms_(0),
    last_calc_video_frame_(0),
    last_calc_audio_frame_(0)
{
}

RtmpProtocol::~RtmpProtocol()
{
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

            cout << LMSG << "fmt:" << (uint16_t)fmt << ",cs_id:" << cs_id << ",io_buffer.Size():" << io_buffer.Size() << endl;

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

                    cout << LMSG << "fmt_0|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
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
                    rtmp_msg.timestamp_calc += timestamp_delta;
                    rtmp_msg.message_length = message_length;
                    rtmp_msg.message_type_id = message_type_id;

                    cout << LMSG << "fmt_1|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
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
                    rtmp_msg.timestamp_calc += timestamp_delta;

                    cout << LMSG << "fmt_2|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 3)
            {
                RtmpMessage& rtmp_msg = csid_head_[cs_id];

                rtmp_msg.timestamp_calc += rtmp_msg.timestamp_delta;

                cout << LMSG << "fmt_3|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
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
                        one_message_done = true;
                    }
                    else
                    {
                        cout << LMSG << "enough chunk data, no enough messge data,message_length:" << rtmp_msg.message_length 
                                     << ",cur_len:" << rtmp_msg.len << ",io_buffer.Size():" << io_buffer.Size() << endl;

                        // 这里也要返回success
                        return kSuccess;
                    }
                }
                else
                {
                    cout << LMSG << "no enough chunk data, io_buffer.Size():" << io_buffer.Size() << endl;
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

            cout << LMSG << "message done|typeid:" << (uint16_t)rtmp_msg.message_type_id << endl;
            cout << LMSG << "media_queue_.size():" << media_queue_.size()
                         << ",audio_queue_.size():" << audio_queue_.size()
                         << ",video_queue_.size():" << video_queue_.size()
                         << endl;

            OnRtmpMessage(rtmp_msg);

            if (rtmp_msg.message_type_id == 8)
            {
                Payload audio_payload(rtmp_msg.msg, rtmp_msg.len);

                media_queue_.push_back(audio_payload);
                audio_queue_.push_back(audio_payload);

                // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
                if (audio_fps_ != 0 && audio_queue_.size() > 10 * audio_fps_)
                {
                    audio_queue_.pop_front();
                }
            }
            else if (rtmp_msg.message_type_id == 9)
            {
                Payload video_payload(rtmp_msg.msg, rtmp_msg.len);

                media_queue_.push_back(video_payload);
                video_queue_.push_back(video_payload);
                //
                // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
                if (video_fps_ != 0 && video_queue_.size() > 10 * video_fps_)
                {
                    video_queue_.pop_front();
                }
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
        if (handshake_status_ == kStatus_0)
        {
            if (io_buffer.Size() >= 1)
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
            static uint32_t s1_len = 4/*time*/ + 4/*zero*/ + 1528/*random*/;
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
            static uint32_t s2_len = 4/*time*/ + 4/*time2*/ + 1528/*random*/;

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

                cout << LMSG << "Handshake done!!!" << endl;
                return kSuccess;
            }
            else
            {
                return kNoEnoughData;
            }
        }
    }


    assert(false);
    // avoid warning
    return kError;
}


int RtmpProtocol::OnRtmpMessage(RtmpMessage& rtmp_msg)
{
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

        case kAudio:
        {
            ++audio_frame_recv_;
        }
        break;

        case kVideo:
        {
            ++video_frame_recv_;
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
                                        SendRtmpMessage(kAmf0Command, data, len);
                                    }
                                }
                            }
                        }
                    }
                    else if (command == "play")
                    {
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
                }
            }
        }
        break;

        case kMetaData:
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
        }
        break;

        default: 
        {
        }
        break;

    }
}

int RtmpProtocol::OnStop()
{
    media_queue_.clear();
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

        cout << LMSG << "[STAT] stream:" << stream_name_ << ",video_fps:" << video_fps_ << ",audio_fps:" << audio_fps_ << ",interval:" << interval << endl;

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

    if (message_type_id == kAmf0Command)
    {
        if (len <= out_chunk_size_)
        {
            chunk_header.WriteU8(fmt << 6 | cs_id);

            message_header.WriteU24(Util::GetNowMs());
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
}
