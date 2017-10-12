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
    last_message_type_id_(0),
    ts_seq_(0),
    ts_couter_(0),
    video_pid_(0x100),
    audio_pid_(0x101),
    pmt_pid_(0xABC),
    audio_continuity_counter_(0),
    video_continuity_counter_(0)
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

void RtmpProtocol::UpdateM3U8()
{
    /*
    #EXTM3U
    #EXT-X-VERSION:3
    #EXT-X-ALLOW-CACHE:NO
    #EXT-X-TARGETDURATION:4
    #EXT-X-MEDIA-SEQUENCE:665
    
    #EXTINF:3.977
    665.ts?wsApp=HLS&wsMonitor=-1
    #EXTINF:3.952
    666.ts?wsApp=HLS&wsMonitor=-1
    #EXTINF:3.387
    667.ts?wsApp=HLS&wsMonitor=-1
    */

    if (ts_queue_.size() <= 5)
    {
        return;
    }

    while (ts_queue_.size() > 5)
    {
        ts_queue_.erase(ts_queue_.begin());
    }

    uint64_t duration = 0;
    for (const auto& ts : ts_queue_)
    {
        double d = ceil(ts.second.duration);

        if (d > duration)
        {
            duration = d;
        }
    }

    ostringstream os;

    os << "#EXTM3U\n"
       << "#EXT-X-VERSION:3\n"
       << "#EXT-X-ALLOW-CACHE:NO\n"
       << "#EXT-X-TARGETDURATION:" << duration << "\n"
       << "#EXT-X-MEDIA-SEQUENCE:" << ts_queue_.begin()->first << "\n"
       << "\n";

    for (const auto& ts : ts_queue_)
    {
        os << "#EXTINF:" << (ts.second.duration) << "\n";
        os << ts.first << ".ts\n";
    }
    
    os << "\n";

    m3u8_ = os.str();

    cout << LMSG << "\n" << TRACE << "\n" << m3u8_ << TRACE << endl;
}

void RtmpProtocol::PacketTs()
{
    cout << LMSG << "last_key_video_frame_:" << last_key_video_frame_ << ",last_key_audio_frame_:" << last_key_audio_frame_
         << ",raw_audio_queue_.size():" << raw_audio_queue_.size() << ",raw_video_queue_.size():" << raw_video_queue_.size()
         << endl;

    auto iter_v_begin = raw_video_queue_.find(last_key_video_frame_);
    auto iter_a_begin = raw_audio_queue_.find(last_key_audio_frame_);

    if (iter_a_begin == raw_audio_queue_.end())
    {
        cout << LMSG << "no any audio frame" << endl;
    }

    if (iter_v_begin == raw_video_queue_.end())
    {
        cout << LMSG << "no any video frame" << endl;

        if (! raw_video_queue_.empty())
        {
            cout << LMSG << "first video:" << raw_video_queue_.begin()->first << endl;
        }
    }

    TsMedia& ts_media = ts_queue_[ts_seq_];


    {
        auto v_end = raw_video_queue_.rbegin();
        ts_media.duration = (v_end->second.GetDts() - iter_v_begin->second.GetDts())/ 1000.0;
    }

    cout << LMSG << "new ts:" << ts_seq_ << ".ts" << ",duration:" << ts_media.duration << endl;

    // PAT
    {
        BitStream bs;
        bs.WriteBytes(1, 0x47);  // sync_byte
        bs.WriteBits(1, 0);      // transport_error_indicator
        bs.WriteBits(1, 1);      // payload_unit_start_indicator 
        bs.WriteBits(1, 0);      // transport_priority
        bs.WriteBits(13, 0);    // pid
        bs.WriteBits(2, 0);              // transport_scrambling_control
        bs.WriteBits(2, 1);
        bs.WriteBits(4, 0);

        bs.WriteBytes(2, 0x0000);
        bs.WriteBits(1, 1);
        bs.WriteBits(1, 0);
        bs.WriteBits(2, 0x03);

        uint16_t length = 13;
        bs.WriteBits(12, length);
        bs.WriteBits(16, 0x0001);
        bs.WriteBits(2, 0x03);
        bs.WriteBits(5, 0);
        bs.WriteBits(1, 1);
        bs.WriteBits(8, 0);
        bs.WriteBits(8, 0);

        bs.WriteBits(16, 0x0001);
        bs.WriteBits(3, 0x07);
        bs.WriteBits(13, pmt_pid_);
        uint32_t crc32 = crc_32_.GetCrc32(bs.GetData() + 5, bs.SizeInBytes() - 5);
        bs.WriteBytes(4, crc32);

        cout << LMSG << "crc32:" << crc32 << endl;

        int left_bytes = 188 - bs.SizeInBytes();

        for (int i = 0; i < left_bytes; ++i)
        {
            bs.WriteBytes(1, 0xFF);
        }

        ts_media.buffer.append((const char*)bs.GetData(), bs.SizeInBytes());
    }

    // PMT
    {
        BitStream bs;
        bs.WriteBytes(1, 0x47);  // sync_byte
        bs.WriteBits(1, 0);      // transport_error_indicator
        bs.WriteBits(1, 1);      // payload_unit_start_indicator 
        bs.WriteBits(1, 0);      // transport_priority
        bs.WriteBits(13, pmt_pid_);    // pid
        bs.WriteBits(2, 0);              // transport_scrambling_control
        bs.WriteBits(2, 1);
        bs.WriteBits(4, 0);

        bs.WriteBytes(2, 0x0002);
        bs.WriteBits(1, 1);
        bs.WriteBits(1, 0);
        bs.WriteBits(2, 0x03);

        uint16_t length = 23;

        bs.WriteBits(12, length);
        // TODO:文档这里都是8bit,但实现得是16bit
        bs.WriteBits(16, 0x0001);
        bs.WriteBits(2, 0x03);
        bs.WriteBits(5, 0);
        bs.WriteBits(1, 1);
        bs.WriteBits(8, 0);
        bs.WriteBits(8, 0);
        bs.WriteBits(3, 0x07);
        bs.WriteBits(13, video_pid_);
        bs.WriteBits(4, 0x0F);
        bs.WriteBits(12, 0);

        bs.WriteBits(8, 0x1b);
        bs.WriteBits(3, 0x07);
        bs.WriteBits(13, video_pid_);
        bs.WriteBits(4, 0x0F);
        bs.WriteBits(12, 0x0000);

        bs.WriteBits(8, 0x0f);
        bs.WriteBits(3, 0x07);
        bs.WriteBits(13, audio_pid_);
        bs.WriteBits(4, 0x0F);
        bs.WriteBits(12, 0x0000);

        // 这里要是5,不能是4,ts header后面多出来的一个字节不知道是啥
        uint32_t crc32 = crc_32_.GetCrc32(bs.GetData() + 5, bs.SizeInBytes() - 5);
        bs.WriteBytes(4, crc32);

        cout << LMSG << "crc32:" << crc32 << endl;

        int left_bytes = 188 - bs.SizeInBytes();

        for (int i = 0; i < left_bytes; ++i)
        {
            bs.WriteBytes(1, 0xFF);
        }

        ts_media.buffer.append((const char*)bs.GetData(), bs.SizeInBytes());
    }

    uint32_t video_count = 0;
    uint32_t audio_count = 0;
    uint32_t frame_key = 0;

    while (iter_v_begin != raw_video_queue_.end() || iter_a_begin != raw_audio_queue_.end())
    {
        WrapPtr payload;
        bool is_video = false;
        if (iter_v_begin == raw_video_queue_.end())
        {
            payload = iter_a_begin->second;
            frame_key = iter_a_begin->first;
            ++iter_a_begin;
            ++audio_count;
        }
        else if (iter_a_begin == raw_audio_queue_.end())
        {
            payload = iter_v_begin->second;
            frame_key = iter_v_begin->first;
            is_video = true;
            ++video_count;
            ++iter_v_begin;
        }
        else
        {
            if (iter_v_begin->second.GetDts() > iter_a_begin->second.GetDts())
            {
                payload = iter_a_begin->second;
                frame_key = iter_a_begin->first;
                ++iter_a_begin;
                ++audio_count;
            }
            else
            {
                payload = iter_v_begin->second;
                frame_key = iter_v_begin->first;
                is_video = true;
                ++video_count;
                ++iter_v_begin;
            }
        }

        int i = 0;
        const uint8_t* data = payload.GetPtr();

        string buffer;

        uint8_t ts_header_size = 4;
        uint8_t adaptation_size = 8;
        uint8_t pes_header_size = 19;

        if (! is_video)
        {
            adaptation_size = 2;
            pes_header_size = 14;
        }

        cout << LMSG << "is video:" << is_video << endl;

        while (i < payload.GetLen())
        {
            uint32_t header_size = ts_header_size;
            uint8_t adaptation_field_control = 1;
            uint8_t adaption_stuffing_bytes = 0;
            uint8_t payload_unit_start_indicator = 0;

            if (i == 0)
            {
                header_size += adaptation_size + pes_header_size;
                payload_unit_start_indicator = 1;
                adaptation_field_control = 3;

                //　音频负载通常小于188,这里要做下处理
                if (payload.GetLen() + ts_header_size + adaptation_size + pes_header_size < 188)
                {
                    if (is_video)
                    {
                        adaption_stuffing_bytes = 188 - (payload.GetLen() + ts_header_size + adaptation_size + pes_header_size + 6/*00 00 00 01 09 BC*/);
                    }
                    else
                    {
                        adaption_stuffing_bytes = 188 - (payload.GetLen() + ts_header_size + adaptation_size + pes_header_size);
                    }
                    header_size += adaption_stuffing_bytes;
                }
            }
            else
            {
                uint32_t left = payload.GetLen() - i;
                
                if (left + ts_header_size + adaptation_size <= 188)
                {
                    header_size += adaptation_size;
                    if (left + ts_header_size + adaptation_size == 188)
                    {
                        header_size -= adaptation_size;
                        adaptation_field_control = 1;
                    }
                    else
                    {
                        adaptation_field_control = 3;
                        adaption_stuffing_bytes = 188 - (left + ts_header_size + adaptation_size);
                        header_size += adaption_stuffing_bytes;
                    }
                }
                else if (left + ts_header_size <= 188)
                {
                    adaptation_field_control = 1;
                }
                else
                {
                    adaptation_field_control = 1;
                }
            }

            BitStream ts_bs;

            // ts header
            ts_bs.WriteBytes(1, 0x47);  // sync_byte
            ts_bs.WriteBits(1, 0);      // transport_error_indicator
            ts_bs.WriteBits(1, payload_unit_start_indicator);      // payload_unit_start_indicator 
            ts_bs.WriteBits(1, 0);      // transport_priority
            if (is_video)
            {
                ts_bs.WriteBits(13, video_pid_);    // pid
            }
            else
            {
                ts_bs.WriteBits(13, audio_pid_);    // pid
            }
            ts_bs.WriteBits(2, 0);              // transport_scrambling_control
            ts_bs.WriteBits(2, adaptation_field_control);

            if (is_video)
            {
                ts_bs.WriteBits(4, GetVideoContinuityCounter());
            }
            else
            {
                ts_bs.WriteBits(4, GetAudioContinuityCounter());
            }

            //cout << LMSG << "i:" << i << ",adaptation_field_control:" << (uint16_t)adaptation_field_control << endl;
            if (adaptation_field_control == 2 || adaptation_field_control == 3)
            {
                // adaption
                uint64_t timestamp = (uint64_t)payload.GetDts() * 90;

                if (is_video)
                {
                    ts_bs.WriteBytes(1, 7 + adaption_stuffing_bytes);
                    ts_bs.WriteBytes(1, 0x50);

                    uint64_t pcr_base = timestamp;
                    uint16_t pcr_ext = 0;

                    ts_bs.WriteBits(33, pcr_base);
                    ts_bs.WriteBits(6, 0x00);
                    ts_bs.WriteBits(9, pcr_ext);
                }
                else
                {
                    ts_bs.WriteBytes(1, 1 + adaption_stuffing_bytes);
                    // audio no pcr
                    ts_bs.WriteBytes(1, 0x40);
                }

                if (adaption_stuffing_bytes > 0)
                {
                    for (uint8_t i = 0; i != adaption_stuffing_bytes; ++i)
                    {
                        ts_bs.WriteBytes(1, 0xFF);
                    }
                }
            }

            if (payload_unit_start_indicator == 1)
            {
                // pes
                ts_bs.WriteBytes(3, (uint32_t)0x000001);
                if (is_video)
                {
                    ts_bs.WriteBytes(1, 0xe0);
                }
                else
                {
                    ts_bs.WriteBytes(1, 0xc0);
                }

                if (is_video)
                {
                    ts_bs.WriteBytes(2, (uint64_t)payload.GetLen());
                }
                else
                {
                    ts_bs.WriteBytes(2, (uint64_t)payload.GetLen() + 3 + 5 + 7);
                }

                ts_bs.WriteBytes(1, 0x80);

                if (is_video)
                {
                    ts_bs.WriteBytes(1, 0xc0);
                    ts_bs.WriteBytes(1, 10);
                }
                else
                {
                    ts_bs.WriteBytes(1, 0x80);
                    ts_bs.WriteBytes(1, 5);
                }

                uint32_t timestamp = payload.GetPts() * 90;

                uint16_t t_32_30 = (timestamp & 0xC0000000) >> 29;
                uint16_t t_29_15 = (timestamp & 0x3FFF8000) >> 15;
                uint16_t t_14_0  = (timestamp & 0x00007FFF);

                // pts
                ts_bs.WriteBits(4, 0x02);
                ts_bs.WriteBits(3, t_32_30);
                ts_bs.WriteBits(1, 1);
                ts_bs.WriteBits(15, t_29_15);
                ts_bs.WriteBits(1, 1);
                ts_bs.WriteBits(15, t_14_0);
                ts_bs.WriteBits(1, 1);

                timestamp = payload.GetDts() * 90;

                t_32_30 = (timestamp & 0xC0000000) >> 29;
                t_29_15 = (timestamp & 0x3FFF8000) >> 15;
                t_14_0  = (timestamp & 0x00007FFF);

                if (is_video)
                {
                    // dts
                    ts_bs.WriteBits(4, 0x02);
                    ts_bs.WriteBits(3, t_32_30);
                    ts_bs.WriteBits(1, 1);
                    ts_bs.WriteBits(15, t_29_15);
                    ts_bs.WriteBits(1, 1);
                    ts_bs.WriteBits(15, t_14_0);
                    ts_bs.WriteBits(1, 1);
                }

                if (is_video)
                {
                    // split nalu
                    ts_bs.WriteBytes(4, 0x00000001);
                    ts_bs.WriteBytes(1, 0x09); // 分隔符
                    ts_bs.WriteBytes(1, 0xBC); // 这个随便填
                    header_size += 6;
                }
            }

            //cout << "header_size:" << header_size << ",ts_bs.SizeInBytes():" << ts_bs.SizeInBytes() << ",is_video:" << is_video << endl;
            assert(header_size == ts_bs.SizeInBytes());

            // SPS PPS before I frame
            if (i == 0 && is_video)
            {
                if (payload.IsIFrame())
                {
                    // nalu type在payload的第一个字节
                    ts_bs.WriteBytes(4, 0x00000001);
                    ts_bs.WriteData(sps_.size(), (const uint8_t*)sps_.data());

                    ts_bs.WriteBytes(4, 0x00000001);
                    ts_bs.WriteData(pps_.size(), (const uint8_t*)pps_.data());

                    ts_bs.WriteBytes(4, 0x00000001);
                    //ts_bs.WriteBytes(1, 0x65);
                    cout << LMSG << frame_key << " I frame" << endl;
                }
                else if (payload.IsPFrame())
                {
                    // P
                    ts_bs.WriteBytes(4, 0x00000001);
                    //ts_bs.WriteBytes(1, 0x41);

                    cout << LMSG << "P frame" << endl;
                }
                else if (payload.IsBFrame())
                {
                    // B
                    ts_bs.WriteBytes(4, 0x00000001);
                    //ts_bs.WriteBytes(1, 0x01);

                    cout << LMSG << "B frame" << endl;
                }
                else
                {
                    cout << LMSG << "Unknown frame:" << (uint16_t)payload.GetFrameType() << endl;
                }
            }

            if (i == 0 && ! is_video && aac_header_.size() > 2)
            {
                // 有卡顿声音的头
                adts_header_[3] |=(uint8_t)((payload.GetLen() & 0x1800) >> 11);           //frame length：value   高2bits
                adts_header_[4] = (uint8_t)((payload.GetLen() & 0x7f8) >> 3);     //frame length:value    中间8bits 
                adts_header_[5] |= (uint8_t)((payload.GetLen() & 0x7) << 5);       //frame length:value    低3bits

                ts_bs.WriteData(7, adts_header_);
            }

            int bytes_left = 188 - ts_bs.SizeInBytes();

            ts_bs.WriteData(bytes_left, data);

            data += bytes_left;
            i += bytes_left;

            ts_media.buffer.append((const char*)ts_bs.GetData(), ts_bs.SizeInBytes());
        }
    }
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

                    string header = aac_header_.substr(2);

                    {   
                        uint8_t audio_object_type = 0;
                        uint8_t sampling_frequency_index = 0;
                        uint8_t channel_config = 0;
                         
                        //audio object type:5bit
                        audio_object_type = header[0] & 0xf8;
                        audio_object_type >>= 3;
                         
                        //sampling frequency index:4bit
                        //高3bits
                        sampling_frequency_index = header[0] & 0x07;
                        sampling_frequency_index <<= 1;
                        //低1bit
                        uint8_t tmp = header[1] & 0x80;
                        tmp >>= 7;
                        sampling_frequency_index |= tmp;

                        cout << LMSG << "sampling_frequency_index:" << (uint16_t)sampling_frequency_index << endl;
                         
                        //channel config:4bits
                        channel_config = header[1] & 0x78;
                        channel_config >>= 3;
                        
                        adts_header_[0] = 0xff;         //syncword:0xfff                          高8bits
                        adts_header_[1] = 0xf0;         //syncword:0xfff                          低4bits
                        adts_header_[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
                        adts_header_[1] |= (0 << 1);    //Layer:0                                 2bits 
                        adts_header_[1] |= 1;           //protection absent:1                     1bit
                        
                        adts_header_[2] = (audio_object_type - 1)<<6;            //profile:audio_object_type - 1                      2bits
                        adts_header_[2] |= (sampling_frequency_index & 0x0f)<<2; //sampling frequency index:sampling_frequency_index  4bits 
                        adts_header_[2] |= (0 << 1);                             //private bit:0                                      1bit
                        adts_header_[2] |= (channel_config & 0x04)>>2;           //channel configuration:channel_config               高1bit
                        
                        adts_header_[3] = (channel_config & 0x03)<<6;     //channel configuration:channel_config      低2bits
                        adts_header_[3] |= (0 << 5);                      //original：0                               1bit
                        adts_header_[3] |= (0 << 4);                      //home：0                                   1bit
                        adts_header_[3] |= (0 << 3);                      //copyright id bit：0                       1bit  
                        adts_header_[3] |= (0 << 2);                      //copyright id start：0                     1bit
                        
                        // ADTS帧长度,有具体帧时再赋值
                        //adts_header_[3] |= ((AdtsLen & 0x1800) >> 11);           //frame length：value   高2bits
                        //adts_header_[4] = (uint8_t)((AdtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits 
                        //adts_header_[5] = (uint8_t)((AdtsLen & 0x7) << 5);       //frame length:value    低3bits

                        adts_header_[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits 
                        adts_header_[6] = 0xfc;
                    }
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
                audio_payload.SetTimestamp(rtmp_msg.timestamp_calc);

                WrapPtr audio_raw_ptr(rtmp_msg.msg + 2, rtmp_msg.len - 2);
                audio_raw_ptr.SetDts(rtmp_msg.timestamp_calc);
                audio_raw_ptr.SetPts(rtmp_msg.timestamp_calc);

                audio_queue_.insert(make_pair(audio_frame_recv_, audio_payload));

                raw_audio_queue_.insert(make_pair(audio_frame_recv_, audio_raw_ptr));

                // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
                if ((audio_fps_ != 0 && audio_queue_.size() > 20 * audio_fps_) || audio_queue_.size() >= 800)
                {
                    audio_queue_.erase(audio_queue_.begin());
                }

                if ((audio_fps_ != 0 && raw_audio_queue_.size() > 20 * audio_fps_) || raw_audio_queue_.size() >= 800)
                {
                    raw_audio_queue_.erase(raw_audio_queue_.begin());
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

                for (auto& player : flv_player_)
                {
                    player->SendFlvMedia(kAudio, true, rtmp_msg.timestamp_calc, rtmp_msg.msg, rtmp_msg.len);
                }
            }
        }

        break;

        case kVideo:
        {
            /*
            RTMP承载的264
            SPS PPS SEI I P B .... I ... P P ...
            5字节FLV头 27 01 00 00 00
            27 01 00 00 00
            如果是AVCHeader,则包含SPS PPS
            第一个I帧包含SEI 
            
            Example:
            ========================================================================================================================
            17 00 00 00 00 01 64 00 1E FF E1 00 19 67 64 00 1E AC D9 40 90 37 B0 11 00 00 03 00 01 00 00 03    ......d......gd....@.7..........
            00 32 0F 16 2D 96 01 00 06 68 EB E3 CB 22 C0
            ========================================================================================================================
			17 00 00 00 00 
			01 64 00 1E FF E1  6个字节可以跳过不关心
			00 19  SPS长度,包含67这个nalu header
			67 64 00 1E AC D9 40 90 37 B0 11 00 00 03 00 01 00 00 03    ......d......gd....@.7..........
			00 32 0F 16 2D 96 
			01  PPS个数
			00 06  PPS长度
            68 EB E3 CB 22 C0

            Example:
            ========================================================================================================================
			17 01 00 00 50 00 00 02 A1 06 05 FF FF 9D DC 45 E9 BD E6 D9 48 B7 96 2C D8 20 D9 23 EE EF 78 32    ....P..........E....H..,. .#..x2
			36 34 20 2D 20 63 6F 72 65 20 31 34 38 20 2D 20 48 2E 32 36 34 2F 4D 50 45 47 2D 34 20 41 56 43    64 - core 148 - H.264/MPEG-4 AVC
			20 63 6F 64 65 63 20 2D 20 43 6F 70 79 6C 65 66 74 20 32 30 30 33 2D 32 30 31 37 20 2D 20 68 74     codec - Copyleft 2003-2017 - ht
			74 70 3A 2F 2F 77 77 77 2E 76 69 64 65 6F 6C 61 6E 2E 6F 72 67 2F 78 32 36 34 2E 68 74 6D 6C 20    tp://www.videolan.org/x264.html 
			2D 20 6F 70 74 69 6F 6E 73 3A 20 63 61 62 61 63 3D 31 20 72 65 66 3D 33 20 64 65 62 6C 6F 63 6B    - options: cabac=1 ref=3 deblock 
			3D 31 3A 30 3A 30 20 61 6E 61 6C 79 73 65 3D 30 78 33 3A 30 78 31 31 33 20 6D 65 3D 68 65 78 20    =1:0:0 analyse=0x3:0x113 me=hex  
			73 75 62 6D 65 3D 37 20 70 73 79 3D 31 20 70 73 79 5F 72 64 3D 31 2E 30 30 3A 30 2E 30 30 20 6D    subme=7 psy=1 psy_rd=1.00:0.00 m
			69 78 65 64 5F 72 65 66 3D 31 20 6D 65 5F 72 61 6E 67 65 3D 31 36 20 63 68 72 6F 6D 61 5F 6D 65    ixed_ref=1 me_range=16 chroma_me
			3D 31 20 74 72 65 6C 6C 69 73 3D 31 20 38 78 38 64 63 74 3D 31 20 63 71 6D 3D 30 20 64 65 61 64    =1 trellis=1 8x8dct=1 cqm=0 dead
			7A 6F 6E 65 3D 32 31 2C 31 31 20 66 61 73 74 5F 70 73 6B 69 70 3D 31 20 63 68 72 6F 6D 61 5F 71    zone=21,11 fast_pskip=1 chroma_q
			70 5F 6F 66 66 73 65 74 3D 2D 32 20 74 68 72 65 61 64 73 3D 31 33 20 6C 6F 6F 6B 61 68 65 61 64    p_offset=-2 threads=13 lookahead
			5F 74 68 72 65 61 64 73 3D 32 20 73 6C 69 63 65 64 5F 74 68 72 65 61 64 73 3D 30 20 6E 72 3D 30    _threads=2 sliced_threads=0 nr=0
			20 64 65 63 69 6D 61 74 65 3D 31 20 69 6E 74 65 72 6C 61 63 65 64 3D 30 20 62 6C 75 72 61 79 5F     decimate=1 interlaced=0 bluray_ 
			63 6F 6D 70 61 74 3D 30 20 63 6F 6E 73 74 72 61 69 6E 65 64 5F 69 6E 74 72 61 3D 30 20 62 66 72    compat=0 constrained_intra=0 bfr
			61 6D 65 73 3D 33 20 62 5F 70 79 72 61 6D 69 64 3D 32 20 62 5F 61 64 61 70 74 3D 31 20 62 5F 62    ames=3 b_pyramid=2 b_adapt=1 b_b
			69 61 73 3D 30 20 64 69 72 65 63 74 3D 31 20 77 65 69 67 68 74 62 3D 31 20 6F 70 65 6E 5F 67 6F    ias=0 direct=1 weightb=1 open_go 
			70 3D 30 20 77 65 69 67 68 74 70 3D 32 20 6B 65 79 69 6E 74 3D 32 35 30 20 6B 65 79 69 6E 74 5F    p=0 weightp=2 keyint=250 keyint_ 
			6D 69 6E 3D 32 35 20 73 63 65 6E 65 63 75 74 3D 34 30 20 69 6E 74 72 61 5F 72 65 66 72 65 73 68    min=25 scenecut=40 intra_refresh
			3D 30 20 72 63 5F 6C 6F 6F 6B 61 68 65 61 64 3D 34 30 20 72 63 3D 63 72 66 20 6D 62 74 72 65 65    =0 rc_lookahead=40 rc=crf mbtree
			3D 31 20 63 72 66 3D 32 33 2E 30 20 71 63 6F 6D 70 3D 30 2E 36 30 20 71 70 6D 69 6E 3D 30 20 71    =1 crf=23.0 qcomp=0.60 qpmin=0 q
			70 6D 61 78 3D 36 39 20 71 70 73 74 65 70 3D 34 20 69 70 5F 72 61 74 69 6F 3D 31 2E 34 30 20 61    pmax=69 qpstep=4 ip_ratio=1.40 a
			71 3D 31 3A 31 2E 30 30 00 80 00 00 08 04 65 88 84 01 FF EE 7A 3B 92 CA FC 25 10 9B 82 C2 FC D8    q=1:1.00......e.....z;...%......
			06 55 F5 19 77 04 90 6A 2B B7 DC F2 71 62 CB AC 39 53 38 C2 68 46 F5 21 51 D9 3F 35 5E 2E 49 1F    .U..w..j+...qb..9S8.hF.!Q.?5^.I.
			96 07 0D F1 04 46 E3 C3 C2 4C 0F 9A BE E0 1C 6A 8E A9 53 7C 8E E5 06 28 6A 78 D9 C0 74 B4 42 49    .....F...L.....j..S|...(jx..t.BI
            ========================================================================================================================
			17 01 00 00 50  FLV头
			00 00 02 A1  NALU长度(这里也可能为00 00 00 01)
			06 NALU类型(06是SEI)

			后面有一个视频帧(I帧)
			71 3D 31 3A 31 2E 30 30 00 80  (...SEI)
			00 00 08 04 NALU长度(这里也可能为00 00 00 01)
			65 (I帧)
            */

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

                    string payload = avc_header_.substr(5);

                    BitBuffer bit_buffer(payload);

                    uint32_t skip = 0;
                    bit_buffer.GetBytes(3, skip);
                    bit_buffer.GetBytes(3, skip);

                    uint16_t sps_len = 0;
                    bit_buffer.GetBytes(2, sps_len);
                    bit_buffer.GetString(sps_len, sps_);

                    uint8_t pps_num = 0;
                    uint16_t pps_len = 0;

                    bit_buffer.GetBytes(1, pps_num);
                    bit_buffer.GetBytes(2, pps_len);

                    bit_buffer.GetString(pps_len, pps_);

                    cout << "SPS" << endl;
                    cout << Util::Bin2Hex(sps_) << endl;

                    cout << "PPS" << endl;
                    cout << Util::Bin2Hex(pps_) << endl;
                }
                else
                {
                    push = true;
                }
            }

            if (push)
            {
                Payload video_payload(rtmp_msg.msg, rtmp_msg.len);
                video_payload.SetTimestamp(rtmp_msg.timestamp_calc);

                uint32_t compositio_time_offset = 0;
                if (rtmp_msg.len >= 5)
                {
                    compositio_time_offset = (rtmp_msg.msg[2] << 16) | (rtmp_msg.msg[3] << 8) | (rtmp_msg.msg[4]);
                    cout << LMSG << "compositio_time_offset:" << compositio_time_offset << endl;

                    if (compositio_time_offset == 0)
                    {
                        video_payload.SetPFrame();
                    }
                    else
                    {
                        video_payload.SetBFrame();
                    }
                }

                uint16_t frame_count = 0;

                if(rtmp_msg.len > 5)
                {
                    uint8_t* data = rtmp_msg.msg + 5;
                    size_t raw_len = rtmp_msg.len - 5;

                    size_t l = 0;
                    while (l < raw_len)
                    {
                        uint32_t nalu_len = (data[l]<<24)|(data[l+1]<<16)|(data[l+2]<<8)|data[l+3];

                        cout << "Nalu length + NALU type:[" << Util::Bin2Hex(data+l, 5) << "]" << endl;

                        uint8_t nalu_header = data[l+4];

                        uint8_t forbidden_zero_bit = (nalu_header & 0x80) >> 7;
                        uint8_t nal_ref_idc = (nalu_header & 0x60) >> 5;
                        uint8_t nalu_unit_type = (nalu_header & 0x1F);

                        cout << "forbidden_zero_bit:" << (uint16_t)forbidden_zero_bit
                             << ",nal_ref_idc:" << (uint16_t)nal_ref_idc
                             << ",nalu_unit_type:" << (uint16_t)nalu_unit_type
                             << endl;

                        bool push_raw = true;

                        WrapPtr video_raw_ptr(data + l + 4, nalu_len);
                        cout << "NALU type + 4byte payload peek:[" << Util::Bin2Hex(data+l+4, 5) << endl;
                        video_raw_ptr.SetDts(rtmp_msg.timestamp_calc);
                        video_raw_ptr.SetPts(rtmp_msg.timestamp_calc);

                        if (nalu_unit_type == 6)
                        {
                            cout << LMSG << "SEI" << endl;
                            push_raw = false;

                            //sei_.assign((const char*)data+l+4, nalu_len);
                        }
                        else if (nalu_unit_type == 5)
                        {
                            cout << LMSG << "IDR" << endl;
                            video_raw_ptr.SetIFrame();
                        }
                        else if (nalu_unit_type == 1)
                        {
                            if (nal_ref_idc == 2)
                            {
                                cout << LMSG << "P" << endl;
                                video_raw_ptr.SetPFrame();
                            }
                            else if (nal_ref_idc == 0)
                            {
                                cout << LMSG << "B" << endl;
                                video_raw_ptr.SetBFrame();
                                video_raw_ptr.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                            }
                            else
                            {
                                if (compositio_time_offset == 0)
                                {
                                    cout << LMSG << "B/P => P" << endl;
                                    video_raw_ptr.SetPFrame();
                                }
                                else
                                {
                                    cout << LMSG << "B/P => B" << endl;
                                    video_raw_ptr.SetBFrame();
                                    video_raw_ptr.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                                }
                            }
                        }
                        else
                        {
                            push_raw = false;
                        }

                        if (push_raw)
                        {
                            cout << LMSG << "insert " << video_frame_recv_ << endl;
                            raw_video_queue_.insert(make_pair(video_frame_recv_ + frame_count, video_raw_ptr));
                            ++frame_count;
                        }

                        l += nalu_len + 4;
                    }
                }


                bool is_key_frame = false;

                // key frame
                if (frame_type == 1)
                {
                    video_payload.SetIFrame();
                    cout << video_frame_recv_ << " I frame" << ",size:" << video_payload.GetLen() << endl;

                    cout << TRACE << endl;
                    cout << Util::Bin2Hex(rtmp_msg.msg, (rtmp_msg.len > 48*100 ? 48*100 : rtmp_msg.len)) << endl;
                    cout << TRACE << endl;

                    is_key_frame = true;

                    if (video_frame_recv_ != 0)
                    {
                        // 打包ts
                        PacketTs();
                    }

                    UpdateM3U8();
                    ++ts_seq_;

                    video_payload.SetKeyFrame();
                    cout << LMSG << "video key frame:" << video_frame_recv_ << "," << audio_frame_recv_ << endl;
                    last_key_video_frame_ = video_frame_recv_;
                    last_key_audio_frame_ = audio_frame_recv_;
                }

                video_queue_.insert(make_pair(video_frame_recv_, video_payload));
                //
                // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
                if ((video_fps_ != 0 && video_queue_.size() > 20 * video_fps_) || video_queue_.size() >= 800)
                {
                    video_queue_.erase(video_queue_.begin());
                }

                if ((video_fps_ != 0 && raw_video_queue_.size() > 20 * video_fps_) || raw_video_queue_.size() >= 800)
                {
                    raw_video_queue_.erase(raw_video_queue_.begin());
                }

                video_frame_recv_ += frame_count;

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

                for (auto& player : flv_player_)
                {
                    player->SendFlvMedia(kVideo, is_key_frame, rtmp_msg.timestamp_calc, rtmp_msg.msg, rtmp_msg.len);
                }
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

int RtmpProtocol::OnNewFlvPlayer(HttpFlvProtocol* protocol)
{
    cout << LMSG << endl;

    protocol->SendFlvHeader();
    protocol->SendFlvMedia(kMetaData,false, 0, (const uint8_t*)metadata_.data(), metadata_.size());
    protocol->SendFlvMedia(kAudio, false, 0, (const uint8_t*)aac_header_.data(), aac_header_.size());
    protocol->SendFlvMedia(kVideo, true, 0, (const uint8_t*)avc_header_.data(), avc_header_.size());

    auto iter_key_video = video_queue_.find(last_key_video_frame_);

    if (iter_key_video != video_queue_.end())
    {
        for (auto it = iter_key_video; it != video_queue_.end(); ++it)
        {
            protocol->SendFlvMedia(kVideo, it->second.IsKeyFrame(), it->second.GetTimestamp(), it->second.GetPtr(), it->second.GetLen());
        }

        auto iter_key_audio = audio_queue_.find(last_key_audio_frame_);

        for (auto it = iter_key_audio; it != audio_queue_.end(); ++it)
        {
            protocol->SendFlvMedia(kAudio, true, it->second.GetTimestamp(), it->second.GetPtr(), it->second.GetLen());
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
        rtmp_src_->RemoveRtmpPlayer(this);
    }
}

int RtmpProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    if (! raw_video_queue_.empty())
    {
        cout << LMSG << "video queue " << raw_video_queue_.begin()->first << "-" << raw_video_queue_.rbegin()->first << endl;
    }

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
