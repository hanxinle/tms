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

#define CDN "ws.upstream.huya.com"
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
    transaction_id_(0.0),
    video_fps_(0),
    audio_fps_(0),
    video_frame_recv_(0),
    audio_frame_recv_(0),
    video_key_frame_count_(0),
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
    pat_continuity_counter_(0),
    pmt_continuity_counter_(0),
    audio_continuity_counter_(0),
    video_continuity_counter_(0)
{
    cout << LMSG << endl;

    adts_header_[0] = 0;
    adts_header_[1] = 0;
    adts_header_[2] = 0;
    adts_header_[3] = 0;
    adts_header_[4] = 0;
    adts_header_[5] = 0;
    adts_header_[6] = 0;
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

            //VERBOSE << LMSG << "fmt:" << (uint16_t)fmt << ",cs_id:" << cs_id << ",io_buffer.Size():" << io_buffer.Size() << endl;

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

                    //VERBOSE << LMSG << "fmt_0|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
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

                    //VERBOSE << LMSG << "fmt_1|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
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

                    //VERBOSE << LMSG << "fmt_2|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 3)
            {
                RtmpMessage& rtmp_msg = csid_head_[cs_id];

                //VERBOSE << LMSG << "fmt_3|" << rtmp_msg.ToString() << ",in_chunk_size_:" << in_chunk_size_ << endl;
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
                        //VERBOSE << LMSG << "enough chunk data, no enough messge data,message_length:" << rtmp_msg.message_length 
                                     //<< ",cur_len:" << rtmp_msg.len << ",io_buffer.Size():" << io_buffer.Size() << endl;

                        // 这里也要返回success
                        return kSuccess;
                    }
                }
                else
                {
                    //VERBOSE << LMSG << "no enough chunk data, io_buffer.Size():" << io_buffer.Size() << endl;
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

            //VERBOSE << LMSG << "message done|typeid:" << (uint16_t)rtmp_msg.message_type_id << endl;
            //VERBOSE << LMSG << ",audio_queue_.size():" << audio_queue_.size()
                         //<< ",video_queue_.size():" << video_queue_.size()
                         //<< endl;

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

void RtmpProtocol::UpdateM3U8()
{
    /*
    #EXTM3U
    #EXT-X-VERSION:3
    #EXT-X-ALLOW-CACHE:NO
    #EXT-X-TARGETDURATION:4
    #EXT-X-MEDIA-SEQUENCE:665
    
    #EXTINF:3.977
    665.ts
    #EXTINF:3.952
    666.ts
    #EXTINF:3.387
    667.ts
    */

    if (ts_queue_.size() <= 3)
    {
        return;
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

    auto riter = ts_queue_.rbegin();

    uint32_t new_ts_seq = riter->first;

    ostringstream os;

    os << "#EXTM3U\n"
       << "#EXT-X-VERSION:3\n"
       << "#EXT-X-ALLOW-CACHE:NO\n"
       << "#EXT-X-TARGETDURATION:" << duration << "\n"
       << "#EXT-X-MEDIA-SEQUENCE:" << new_ts_seq << "\n"
       << "#EXTINF:" << ts_queue_[new_ts_seq - 2].duration << "\n"
       << (new_ts_seq - 2) << ".ts\n"
       << "#EXTINF:" << ts_queue_[new_ts_seq - 1].duration << "\n"
       << (new_ts_seq - 1) << ".ts\n"
       << "#EXTINF:" << ts_queue_[new_ts_seq].duration << "\n"
       << new_ts_seq << ".ts\n";
    
    os << "\n";

    m3u8_ = os.str();

    cout << LMSG << "\n" << TRACE << "\n" << m3u8_ << TRACE << endl;
}

void RtmpProtocol::PacketTs(const Payload& payload)
{
    if (ts_queue_.count(ts_seq_) == 0)
    {
        ts_queue_[ts_seq_].ts_data.reserve(1024*64);

        ts_queue_[ts_seq_].ts_data.append(PacketTsPat());
        ts_queue_[ts_seq_].ts_data.append(PacketTsPmt());
        ts_queue_[ts_seq_].first_dts = payload.GetDts();
    }

    ts_queue_[ts_seq_].duration = (payload.GetDts() - ts_queue_[ts_seq_].first_dts) / 1000.0;

    const uint8_t* data = payload.GetRawData();

    bool is_video = payload.IsVideo();

    uint8_t ts_header_size = 4;
    uint8_t adaptation_size = 8;
    uint8_t pes_header_size = 19;

    if (! is_video)
    {
        adaptation_size = 2;
        pes_header_size = 14;
    }

    int i = 0;
    while (i < payload.GetRawLen())
    {
        uint32_t header_size = ts_header_size;
        uint8_t adaptation_field_control = 1; // 1:无自适应区 2.只有自适应区 3.同时有负载和自适应区
        uint8_t adaption_stuffing_bytes = 0;
        uint8_t payload_unit_start_indicator = 0; // 帧首包标识

        if (i == 0)
        {
            header_size += pes_header_size;
            payload_unit_start_indicator = 1;

            if (is_video)
            {
                header_size += adaptation_size;
                adaptation_field_control = 3;
            }
            else
            {
                adaptation_field_control = 1;
            }

            //　音频负载通常小于188,这里要做下处理
            if (payload.GetRawLen() + ts_header_size + adaptation_size + pes_header_size < 188)
            {
                if (is_video)
                {
                    adaption_stuffing_bytes = 188 - (payload.GetRawLen() + ts_header_size + adaptation_size + pes_header_size + 6/*00 00 00 01 09 BC*/);
                }
                else
                {
                    adaption_stuffing_bytes = 188 - (payload.GetRawLen() + ts_header_size + adaptation_size + pes_header_size);
                }

                if (! is_video)
                {
                    header_size += adaptation_size;
                    adaptation_field_control = 3;
                }

                header_size += adaption_stuffing_bytes;
            }
        }
        else
        {
            uint32_t left = payload.GetRawLen() - i;
            
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

        if (adaptation_field_control == 2 || adaptation_field_control == 3)
        {
            // adaption
            uint64_t timestamp = (uint64_t)payload.GetDts() * 90;

            if (is_video)
            {
                if (payload_unit_start_indicator == 1)
                {
                    // 视频而且是帧的首个包才需要PCR
                    ts_bs.WriteBytes(1, 7 + adaption_stuffing_bytes);
                    ts_bs.WriteBytes(1, 0x10);
                }
                else
                {
                    ts_bs.WriteBytes(1, 1 + adaption_stuffing_bytes);
                    ts_bs.WriteBytes(1, 0x00);
                }


                if (payload_unit_start_indicator == 1)
                {
                    uint64_t pcr_base = timestamp;
                    uint16_t pcr_ext = 0;

                    ts_bs.WriteBits(33, pcr_base);
                    ts_bs.WriteBits(6, 0x00);
                    ts_bs.WriteBits(9, pcr_ext);
                }
                else
                {
                    header_size -= 6;
                }
            }
            else
            {
                // 音频不需要PCR
                ts_bs.WriteBytes(1, 1 + adaption_stuffing_bytes);
                // audio no pcr
                ts_bs.WriteBytes(1, 0x00);
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
                // 视频的PES长度这里随便填,无所谓
                ts_bs.WriteBytes(2, 0x0000);
            }
            else
            {
                // 音频的一定是音频负载长度+3(PES后面3个flag)+5(只有DTS)+7(adts头长度)
                ts_bs.WriteBytes(2, (uint64_t)payload.GetRawLen() + 3 + 5 + 7);
            }

            ts_bs.WriteBytes(1, 0x80);

            uint32_t pts = payload.GetPts() * 90;
            uint32_t dts = payload.GetDts() * 90;

            if (is_video)
            {
                ts_bs.WriteBytes(1, 0xc0);
                ts_bs.WriteBytes(1, 10);
            }
            else
            {
                // 音频只需要PTS即可
                ts_bs.WriteBytes(1, 0x80);
                ts_bs.WriteBytes(1, 5);
            }

            uint16_t t_32_30 = (pts & 0xC0000000) >> 29;
            uint16_t t_29_15 = (pts & 0x3FFF8000) >> 15;
            uint16_t t_14_0  = (pts & 0x00007FFF);

            // pts
            ts_bs.WriteBits(4, 0x02);
            ts_bs.WriteBits(3, t_32_30);
            ts_bs.WriteBits(1, 1);
            ts_bs.WriteBits(15, t_29_15);
            ts_bs.WriteBits(1, 1);
            ts_bs.WriteBits(15, t_14_0);
            ts_bs.WriteBits(1, 1);

            t_32_30 = (dts & 0xC0000000) >> 29;
            t_29_15 = (dts & 0x3FFF8000) >> 15;
            t_14_0  = (dts & 0x00007FFF);

            if (is_video)
            {
                // XXX:DTS跟PTS一样的话要不要打还未知
                //if (dts != pts)

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
                //else
                //{
                //    header_size -= 5;
                //}
            }

            if (is_video)
            {
                // split nalu
                ts_bs.WriteBytes(4, 0x00000001);
                ts_bs.WriteBytes(1, 0x09); // 分隔符
                ts_bs.WriteBytes(1, 0x10); // 这个随便填
                header_size += 6;
            }
        }

        //VERBOSE << LMSG << "header_size:" << header_size << ",ts_bs.SizeInBytes():" << ts_bs.SizeInBytes() << ",is_video:" << is_video << endl;
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
                //VERBOSE << LMSG << frame_key << " I frame" << endl;
            }
            else if (payload.IsPFrame())
            {
                // P
                ts_bs.WriteBytes(4, 0x00000001);
                //ts_bs.WriteBytes(1, 0x41);

                //VERBOSE << LMSG << "P frame" << endl;
            }
            else if (payload.IsBFrame())
            {
                // B
                ts_bs.WriteBytes(4, 0x00000001);
                //ts_bs.WriteBytes(1, 0x01);

                //VERBOSE << LMSG << "B frame" << endl;
            }
            else
            {
                //VERBOSE << LMSG << "Unknown frame:" << (uint16_t)payload.GetFrameType() << endl;
            }
        }

        if (i == 0 && ! is_video && aac_header_.size() >= 2)
        {
            uint16_t adts_len = (uint16_t)payload.GetRawLen() + 7;

            adts_header_[3] &= 0xFC;
            adts_header_[3] |=(uint8_t)((adts_len & 0x1800) >> 11);         //frame length:value,高2bits
            adts_header_[4] = (uint8_t)((adts_len & 0x7f8) >> 3);           //frame length:value,中间8bits 
            adts_header_[5] &= 0x1F;
            adts_header_[5] |= ((uint8_t)((adts_len & 0x07) << 5) & 0xe0);  //frame length:value,低3bits

            uint16_t calc_len = ((adts_header_[3] & 0x03) << 11) | ((uint16_t)adts_header_[4] << 3) | ((adts_header_[5] & 0xE0) >> 5);

            uint64_t tmp = adts_len << 13;

            assert(adts_len == calc_len);

            ts_bs.WriteData(7, adts_header_);
        }

        int bytes_left = 188 - ts_bs.SizeInBytes();

        ts_bs.WriteData(bytes_left, data);

        ts_queue_[ts_seq_].ts_data.append((const char*)ts_bs.GetData(), ts_bs.SizeInBytes());

        data += bytes_left;
        i += bytes_left;
    }
}

string& RtmpProtocol::PacketTsPat()
{
    if (! ts_pat_.empty())
    {
        return ts_pat_;
    }

    BitStream bs;

    // ts header
    bs.WriteBytes(1, 0x47);  // sync_byte
    bs.WriteBits(1, 0);      // transport_error_indicator
    bs.WriteBits(1, 1);      // payload_unit_start_indicator 
    bs.WriteBits(1, 0);      // transport_priority
    bs.WriteBits(13, 0);    // pid
    bs.WriteBits(2, 0);              // transport_scrambling_control
    bs.WriteBits(2, 1);
    bs.WriteBits(4, GetPatContinuityCounter());

    // table id
    bs.WriteBytes(2, 0x0000); // XXX:标准是1个字节,但实现看起来都是2个字节
    bs.WriteBits(1, 1);
    bs.WriteBits(1, 0);
    bs.WriteBits(2, 0x03);

    uint16_t length = 13;
    bs.WriteBits(12, length); // 后面数据长度,不包括这个字节

    bs.WriteBits(16, 0x0001);
    bs.WriteBits(2, 0x03);
    bs.WriteBits(5, 0);
    bs.WriteBits(1, 1);
    bs.WriteBits(8, 0);
    bs.WriteBits(8, 0);

    bs.WriteBits(16, 0x0001);
    bs.WriteBits(3, 0x07);
    bs.WriteBits(13, pmt_pid_);
    // CRC32从table_id开始(包括table_id), 这里+5是因为上面的table_id用了2个字节,标准是1个字节
    uint32_t crc32 = crc_32_.GetCrc32(bs.GetData() + 5, bs.SizeInBytes() - 5);
    bs.WriteBytes(4, crc32);

    int left_bytes = 188 - bs.SizeInBytes();

    for (int i = 0; i < left_bytes; ++i)
    {
        bs.WriteBytes(1, 0xFF);
    }

    ts_pat_.assign((const char*)bs.GetData(), bs.SizeInBytes());

    return ts_pat_;
}

string& RtmpProtocol::PacketTsPmt()
{
    if (! ts_pmt_.empty())
    {
        return ts_pmt_;
    }

    BitStream bs;

    // ts header
    bs.WriteBytes(1, 0x47);  // sync_byte
    bs.WriteBits(1, 0);      // transport_error_indicator
    bs.WriteBits(1, 1);      // payload_unit_start_indicator 
    bs.WriteBits(1, 0);      // transport_priority
    bs.WriteBits(13, pmt_pid_);    // pid
    bs.WriteBits(2, 0);              // transport_scrambling_control
    bs.WriteBits(2, 1);
    bs.WriteBits(4, GetPmtContinuityCounter());

    // TODO:文档这里都是8bit,但实现得是16bit
    bs.WriteBytes(2, 0x0002);
    bs.WriteBits(1, 1);
    bs.WriteBits(1, 0);
    bs.WriteBits(2, 0x03);

    uint16_t length = 23;

    bs.WriteBits(12, length);
    bs.WriteBits(16, 0x0001);
    bs.WriteBits(2, 0x03);
    bs.WriteBits(5, 0);
    bs.WriteBits(1, 1);
    bs.WriteBits(8, 0);
    bs.WriteBits(8, 0);
    bs.WriteBits(3, 0x07);
    bs.WriteBits(13, video_pid_); // PCR所在的PID,指定为视频pid
    bs.WriteBits(4, 0x0F);
    bs.WriteBits(12, 0);

    bs.WriteBits(8, 0x1b); // 0x1b h264
    bs.WriteBits(3, 0x07);
    bs.WriteBits(13, video_pid_);
    bs.WriteBits(4, 0x0F);
    bs.WriteBits(12, 0x0000);

    bs.WriteBits(8, 0x0f); // 0x0f aac
    bs.WriteBits(3, 0x07);
    bs.WriteBits(13, audio_pid_);
    bs.WriteBits(4, 0x0F);
    bs.WriteBits(12, 0x0000);

    // 这里要是5,不能是4,ts header后面多出来的一个字节不知道是啥
    uint32_t crc32 = crc_32_.GetCrc32(bs.GetData() + 5, bs.SizeInBytes() - 5);
    bs.WriteBytes(4, crc32);

    int left_bytes = 188 - bs.SizeInBytes();

    for (int i = 0; i < left_bytes; ++i)
    {
        bs.WriteBytes(1, 0xFF);
    }

    ts_pmt_.assign((const char*)bs.GetData(), bs.SizeInBytes());

    return ts_pmt_;
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

        if (audio_frame_recv_ <= 10)
        {
            cout << LMSG << "audio_frame_recv_:" << audio_frame_recv_
                 << ",sound_format:" << (int)sound_format
                 << ",sound_rate:" << (int)sound_rate
                 << ",sound_size:" << (int)sound_size
                 << ",sound_type:" << (int)sound_type
                 << ",aac_packet_type:" << (int)aac_packet_type
                 << endl;
        }

        if (sound_format == 10)
        {
            if (aac_packet_type == 0)
            {
                aac_header_.assign((const char*)rtmp_msg.msg, rtmp_msg.len);
                aac_header_ = aac_header_.substr(2);
                cout << LMSG << "recv aac_header_" << ",size:" << aac_header_.size() << endl;
                cout << Util::Bin2Hex(rtmp_msg.msg, rtmp_msg.len) << endl;

                uint8_t audio_object_type = 0;
                uint8_t sampling_frequency_index = 0;
                uint8_t channel_config = 0;
                 
                //audio object type:5bit
                audio_object_type = aac_header_[0] & 0xf8;
                audio_object_type >>= 3;
                 
                //sampling frequency index:4bit
                //高3bits
                sampling_frequency_index = aac_header_[0] & 0x07;
                sampling_frequency_index <<= 1;
                //低1bit
                uint8_t tmp = aac_header_[1] & 0x80;
                tmp >>= 7;
                sampling_frequency_index |= tmp;

                cout << LMSG << "sampling_frequency_index:" << (uint16_t)sampling_frequency_index << endl;
                 
                //channel config:4bits
                channel_config = aac_header_[1] & 0x78;
                channel_config >>= 3;
                
                adts_header_[0] = 0xff;         //syncword:0xfff                          高8bits
                adts_header_[1] = 0xf0;         //syncword:0xfff                          低4bits
                adts_header_[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
                adts_header_[1] |= (0 << 1);    //Layer:0                                 2bits 
                adts_header_[1] |= 1;           //protection absent:1                     1bit
                                                // set to 1 if there is no CRC and 0 if there is CRC
                
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
            else
            {
                insert_to_aq = true;
            }
        }
    }
    else
    {
        cout << LMSG << "impossible?" << endl;
        assert(false);
    }

    if (insert_to_aq)
    {
        uint8_t* audio_raw_data = (uint8_t*)malloc(rtmp_msg.len);
        memcpy(audio_raw_data, rtmp_msg.msg, rtmp_msg.len);

        Payload audio_payload(audio_raw_data, rtmp_msg.len);
        audio_payload.SetAudio();
        audio_payload.SetDts(rtmp_msg.timestamp_calc);
        audio_payload.SetPts(rtmp_msg.timestamp_calc);

        audio_queue_.insert(make_pair(audio_frame_recv_, audio_payload));

        PacketTs(audio_payload);

        ++audio_frame_recv_;

        // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
        if (((audio_fps_ != 0 && audio_queue_.size() > 20 * audio_fps_) || audio_queue_.size() >= 800) && ts_queue_.size() > 10)
        {
            audio_queue_.erase(audio_queue_.begin());
        }

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
    }

    return kSuccess;
}

int RtmpProtocol::OnVideo(RtmpMessage& rtmp_msg)
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

    bool insert_to_vq = false;
    uint8_t frame_type = 0xff;
    uint8_t codec_id = 0xff;
    uint8_t avc_packet_type = 0xff;

    if (rtmp_msg.len >= 2)
    {
        BitBuffer bit_buffer(rtmp_msg.msg, 2);

        bit_buffer.GetBits(4, frame_type);
        bit_buffer.GetBits(4, codec_id);
        bit_buffer.GetBits(8, avc_packet_type);


        //VERBOSE << LMSG << "video_frame_recv_:" << video_frame_recv_
        //        << ",frame_type:" << (int)frame_type 
        //        << ",codec_id:" << (int)codec_id
        //        << ",avc_packet_type:" << (int)avc_packet_type
        //        << endl;

        if (codec_id == 7)
        {
            if (avc_packet_type == 0)
            {
                ParseAvcHeader(rtmp_msg);
            }
            else
            {
                insert_to_vq = true;
            }
        }
    }

    if (insert_to_vq)
    {
        uint32_t compositio_time_offset = 0;
        if (rtmp_msg.len > 5)
        {
            compositio_time_offset = (rtmp_msg.msg[2] << 16) | (rtmp_msg.msg[3] << 8) | (rtmp_msg.msg[4]);
            //cout << LMSG << "compositio_time_offset:" << compositio_time_offset << endl;

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

                //cout << LMSG << "Nalu length + NALU type:[" << Util::Bin2Hex(data+cur_len, 5) << "]" << endl;

                uint8_t nalu_header = data[cur_len+4];

                uint8_t forbidden_zero_bit = (nalu_header & 0x80) >> 7;
                uint8_t nal_ref_idc = (nalu_header & 0x60) >> 5;
                uint8_t nalu_unit_type = (nalu_header & 0x1F);

                //cout << LMSG << "forbidden_zero_bit:" << (uint16_t)forbidden_zero_bit
                //     << ",nal_ref_idc:" << (uint16_t)nal_ref_idc
                //     << ",nalu_unit_type:" << (uint16_t)nalu_unit_type
                //     << endl;

                insert_to_vq = false;

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
                    insert_to_vq = false;
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
                    insert_to_vq = true;
                    cout << LMSG << "IDR" << endl;
                    video_payload.SetIFrame();
                    video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);

                    if (last_key_video_frame_ != 0)
                    {
                        // 打包ts
                        UpdateM3U8();
                        ++ts_seq_;
                    }

                    ++video_key_frame_count_;

                    last_key_video_frame_ = video_frame_recv_;
                    last_key_audio_frame_ = audio_frame_recv_;
                }
                else if (nalu_unit_type == 1)
                {
                    insert_to_vq = true;

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
                    insert_to_vq = false;
                }

                if (insert_to_vq)
                {
                    //cout << LMSG << "insert " << video_frame_recv_ << endl;
                    video_queue_.insert(make_pair(video_frame_recv_, video_payload));

                    PacketTs(video_payload);

                    ++video_frame_recv_;

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
                }

                cur_len += nalu_len + 4;
            }

            assert(cur_len == raw_len);
        }

        // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
        if (((video_fps_ != 0 && video_queue_.size() > 20 * video_fps_) || video_queue_.size() >= 800) && ts_queue_.size() > 10)
        {
            if (video_queue_.begin()->second.IsIFrame())
            {
                cout << LMSG << "erase " << ts_queue_.begin()->first << ".ts" << endl;
                ts_queue_.erase(ts_queue_.begin());
            }

            video_queue_.erase(video_queue_.begin());
        }
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
                                cout << LMSG << "stream_name_:" << stream_name_ << endl;
                                if (stream_mgr_->RegisterStream(app_, stream_name_, this) == false)
                                {
                                    cout << LMSG << "error" << endl;
                                    return kError;
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
            RtmpProtocol* rtmp_publisher = stream_mgr_->GetRtmpProtocolByAppStream(app_, stream_name_);
            if (rtmp_publisher == NULL)
            {
            }
            else
            {
                uint8_t* data = NULL;
                int len = output.Read(data, output.Size());

                if (data != NULL && len > 0)
                {
                    SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
                }

                SetRtmpSrc(rtmp_publisher);
                rtmp_publisher->AddRtmpPlayer(this);
            }
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
            if (amf_command[3]->GetString(stream_name_))
            {
                cout << LMSG << "stream_name:" << stream_name_ << endl;

                if (stream_mgr_->RegisterStream(app_, stream_name_, this) == false)
                {
                    cout << LMSG << "error" << endl;
                    return kError;
                }
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
            rtmp_src_->OnNewRtmpPlayer(this);
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnMetaData(RtmpMessage& rtmp_msg)
{
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

    return kSuccess;
}

int RtmpProtocol::ParseAvcHeader(RtmpMessage& rtmp_msg)
{
    avc_header_.assign((const char*)rtmp_msg.msg, rtmp_msg.len);
    avc_header_ = avc_header_.substr(5);

    cout << LMSG << "recv avc_header_" << ",size:" << avc_header_.size() << endl;
    cout << Util::Bin2Hex(rtmp_msg.msg, rtmp_msg.len) << endl;

    BitBuffer bit_buffer(avc_header_);

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

int RtmpProtocol::OnNewRtmpPlayer(RtmpProtocol* protocol)
{
    cout << LMSG << endl;

    SendRtmpMessage(6, 1, kMetaData, (const uint8_t*)metadata_.data(), metadata_.size());

    if (! aac_header_.empty())
    {
        string audio_header;
        audio_header.append(1, 0xAF);
        audio_header.append(1, 0x00);
        audio_header.append(aac_header_);

        protocol->SendRtmpMessage(4, 1, kAudio, (const uint8_t*)audio_header.data(), audio_header.size());
    }

    if (! avc_header_.empty())
    {
        string video_header;
        video_header.append(1, 0x17);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(avc_header_);

        protocol->SendRtmpMessage(6, 1, kVideo, (const uint8_t*)video_header.data(), video_header.size());
    }

    auto iter_video = video_queue_.find(last_key_video_frame_);
    auto iter_audio = audio_queue_.find(last_key_audio_frame_);

    while (iter_audio != audio_queue_.end() || iter_video != video_queue_.end())
    {
        if (iter_audio == audio_queue_.end())
        {
            cout << LMSG << endl;
            protocol->SendMediaData(iter_video->second);
            ++iter_video;
        }
        else if (iter_video == video_queue_.end())
        {
            cout << LMSG << endl;
            protocol->SendMediaData(iter_audio->second);
            ++iter_audio;
        }
        else
        {
            if (iter_audio->second.GetDts() > iter_video->second.GetDts())
            {
                cout << LMSG << endl;
                protocol->SendMediaData(iter_video->second);
                ++iter_video;
            }
            else
            {
                cout << LMSG << endl;
                protocol->SendMediaData(iter_audio->second);
                ++iter_audio;
            }
        }
    }
}

int RtmpProtocol::OnNewFlvPlayer(HttpFlvProtocol* protocol)
{
    cout << LMSG << endl;

    protocol->SendFlvHeader();

    if (! metadata_.empty())
    {
        protocol->SendFlvMetaData(metadata_);
    }

    if (! avc_header_.empty())
    {
        protocol->SendFlvVideoHeader(avc_header_);
    }

    if (! aac_header_.empty())
    {
        protocol->SendFlvAudioHeader(aac_header_);
    }

#if 0
    auto iter_key_video = video_queue_.find(last_key_video_frame_);

    for (auto it = iter_key_video; it != video_queue_.end(); ++it)
    {
        protocol->SendFlvVideo(it->second);
    }

    auto iter_key_audio = audio_queue_.find(last_key_audio_frame_);

    for (auto it = iter_key_audio; it != audio_queue_.end(); ++it)
    {
        protocol->SendFlvAudio(it->second);
    }
#else
    auto iter_video = video_queue_.find(last_key_video_frame_);
    auto iter_audio = audio_queue_.find(last_key_audio_frame_);

    while (iter_audio != audio_queue_.end() || iter_video != video_queue_.end())
    {
        if (iter_audio == audio_queue_.end())
        {
            cout << LMSG << endl;
            protocol->SendFlvVideo(iter_video->second);
            ++iter_video;
        }
        else if (iter_video == video_queue_.end())
        {
            cout << LMSG << endl;
            protocol->SendFlvAudio(iter_audio->second);
            ++iter_audio;
        }
        else
        {
            if (iter_audio->second.GetDts() > iter_video->second.GetDts())
            {
                cout << LMSG << endl;
                protocol->SendFlvVideo(iter_video->second);
                ++iter_video;
            }
            else
            {
                cout << LMSG << endl;
                protocol->SendFlvAudio(iter_audio->second);
                ++iter_audio;
            }
        }
    }
#endif
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
    cout << LMSG << "rtmp_forwards_.size():" << rtmp_forwards_.size() << ", rtmp_player_.size():" << rtmp_player_.size() << endl;

    if (! video_queue_.empty())
    {
        cout << LMSG << "video queue:" << video_queue_.size() << " [" << video_queue_.begin()->first << "-" << video_queue_.rbegin()->first << "]" << endl;
    }

    if (! audio_queue_.empty())
    {
        cout << LMSG << "audio queue:" << audio_queue_.size() << " [" << audio_queue_.begin()->first << "-" << audio_queue_.rbegin()->first << "]" << endl;
    }

    cout << LMSG << "ts queue:" << ts_queue_.size() << endl;

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

int RtmpProtocol::SendRtmpMessage(const uint32_t cs_id, const uint32_t& message_stream_id, const uint8_t& message_type_id, const uint8_t* data, const size_t& len)
{
    RtmpMessage rtmp_message;

    rtmp_message.cs_id = cs_id;
    rtmp_message.timestamp = 0;
    rtmp_message.timestamp_delta = 0;
    rtmp_message.message_length = len;
    rtmp_message.message_type_id = message_type_id;

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

    uint32_t cur_timestamp       = cur_info.timestamp;
    uint32_t cur_timestamp_delta = cur_info.timestamp_delta;
    uint32_t cur_message_length  = cur_info.message_length;
    uint8_t  cur_message_type_id = cur_info.message_type_id;

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
                header.WriteU32(0x10000000);
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

int RtmpProtocol::ConnectForwardServer(const string& ip, const uint16_t& port)
{
    int fd = CreateNonBlockTcpSocket();

    if (fd < 0)
    {
        cout << LMSG << "ConnectForwardServer ret:" << fd << endl;
        return -1;
    }

    int ret = ConnectHost(fd, ip, port);

    if (ret < 0 && errno != EINPROGRESS)
    {
        cout << LMSG << "Connect ret:" << ret << endl;
        return -1;
    }

    Fd* socket = new TcpSocket(epoller_, fd, (SocketHandle*)stream_mgr_);

    RtmpProtocol* rtmp_dst = stream_mgr_->GetOrCreateProtocol(*socket);

    rtmp_dst->SetApp(app_);
    rtmp_dst->SetStreamName(stream_name_);
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
