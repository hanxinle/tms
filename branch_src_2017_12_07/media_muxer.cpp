#include <math.h>

#include "bit_buffer.h"
#include "bit_stream.h"
#include "media_muxer.h"
#include "util.h"

using namespace std;
using namespace socket_util;

MediaMuxer::MediaMuxer()
    :
    video_frame_id_(0),
    audio_frame_id_(0),
    video_frame_recv_count_(0),
    audio_frame_recv_count_(0),
    video_key_frame_recv_count_(0),
    pre_video_key_frame_id_(0),
    pre_audio_key_frame_id_(0),
    video_calc_fps_(0),
    audio_calc_fps_(0),
    pre_calc_fps_ms_(0),
    forward_toggle_bit_(false),
    ts_seq_(0),
    ts_couter_(0),
    ts_video_pid_(0x100),
    ts_audio_pid_(0x101),
    ts_pmt_pid_(0xABC),
    ts_pat_continuity_counter_(0),
    ts_pmt_continuity_counter_(0),
    ts_audio_continuity_counter_(0),
    ts_video_continuity_counter_(0)
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

MediaMuxer::~MediaMuxer()
{
    cout << LMSG << endl;
}

void MediaMuxer::UpdateM3U8()
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

    if (ts_queue_.size() < 3)
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

void MediaMuxer::PacketTs(const Payload& payload)
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
        uint8_t extern_data_len = 0;

        if (i == 0)
        {
            header_size += pes_header_size;
            payload_unit_start_indicator = 1;

            if (is_video)
            {
                header_size += adaptation_size;
                adaptation_field_control = 3;
                extern_data_len = 6; // 添加的nalu分隔符/*00 00 00 01 09 BC*/
            }
            else
            {
                adaptation_field_control = 1;
                extern_data_len = 7; // ADTS
            }

            //　音频负载通常小于188,这里要做下处理
            if (payload.GetRawLen() + ts_header_size + adaptation_size + pes_header_size + extern_data_len < 188)
            {
                if (is_video)
                {
                    adaption_stuffing_bytes = 188 - (payload.GetRawLen() + ts_header_size + adaptation_size + pes_header_size + extern_data_len/*00 00 00 01 09 BC*/);
                }
                else
                {
                    adaption_stuffing_bytes = 188 - (payload.GetRawLen() + ts_header_size + adaptation_size + pes_header_size + extern_data_len);
                }

                cout << LMSG << "payload size:" << payload.GetRawLen() << ", ts_header_size:" << (int)ts_header_size << ", adaptation_size:" << (int)adaptation_size
                             << ",pes_header_size:" << (int)pes_header_size << ",extern_data_len:" << (int)extern_data_len << endl;

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
            ts_bs.WriteBits(13, ts_video_pid_);    // pid
        }
        else
        {
            ts_bs.WriteBits(13, ts_audio_pid_);    // pid
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
                    uint64_t dts = payload.GetDts() / 1000; // unit: second

                    // REF:http://blog.csdn.net/xiaojun111111/article/details/40583075
                    uint64_t pcr_base = (27000000UL * dts / 300) % (2UL<<33);
                    uint16_t pcr_ext = (27000000UL * dts / 1) % 300;

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

            uint64_t pts = payload.GetPts() * 90;
            uint64_t dts = payload.GetDts() * 90;

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

            uint16_t t_32_30 = (pts & 0x00000001C0000000) >> 30;
            uint16_t t_29_15 = (pts & 0x000000003FFF8000) >> 15;
            uint16_t t_14_0  = (pts & 0x0000000000007FFF);

            // pts
            ts_bs.WriteBits(4, 0x02);
            ts_bs.WriteBits(3, t_32_30);
            ts_bs.WriteBits(1, 1);
            ts_bs.WriteBits(15, t_29_15);
            ts_bs.WriteBits(1, 1);
            ts_bs.WriteBits(15, t_14_0);
            ts_bs.WriteBits(1, 1);

            t_32_30 = (dts & 0x00000001C0000000) >> 30;
            t_29_15 = (dts & 0x000000003FFF8000) >> 15;
            t_14_0  = (dts & 0x0000000000007FFF);

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

        if (i == 0 && ! is_video && audio_header_.size() >= 2)
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

        assert(bytes_left > 0);

        ts_bs.WriteData(bytes_left, data);

        assert(ts_bs.SizeInBytes() == 188);
        ts_queue_[ts_seq_].ts_data.append((const char*)ts_bs.GetData(), ts_bs.SizeInBytes());

        data += bytes_left;
        i += bytes_left;
    }
}

string& MediaMuxer::PacketTsPat()
{
    if (ts_pat_.size() >= 4)
    {
        ts_pat_[3] = (char)(0x10 | GetPatContinuityCounter());
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
    bs.WriteBits(13, ts_pmt_pid_);
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

string& MediaMuxer::PacketTsPmt()
{
    if (ts_pmt_.size() >= 4)
    {
        ts_pmt_[3] = (char)(0x10 | GetPmtContinuityCounter());
        return ts_pmt_;
    }

    BitStream bs;

    // ts header
    bs.WriteBytes(1, 0x47);  // sync_byte
    bs.WriteBits(1, 0);      // transport_error_indicator
    bs.WriteBits(1, 1);      // payload_unit_start_indicator 
    bs.WriteBits(1, 0);      // transport_priority
    bs.WriteBits(13, ts_pmt_pid_);    // pid
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
    bs.WriteBits(13, ts_video_pid_); // PCR所在的PID,指定为视频pid
    bs.WriteBits(4, 0x0F);
    bs.WriteBits(12, 0);

    bs.WriteBits(8, 0x1b); // 0x1b h264
    bs.WriteBits(3, 0x07);
    bs.WriteBits(13, ts_video_pid_);
    bs.WriteBits(4, 0x0F);
    bs.WriteBits(12, 0x0000);

    bs.WriteBits(8, 0x0f); // 0x0f aac
    bs.WriteBits(3, 0x07);
    bs.WriteBits(13, ts_audio_pid_);
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

int MediaMuxer::OnAudio(Payload& audio_payload)
{
    audio_queue_.insert(make_pair(audio_frame_id_, audio_payload));

    PacketTs(audio_payload);

    ++audio_frame_recv_count_;
    ++audio_frame_id_;
    ++audio_calc_fps_;

    // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
    if (((audio_calc_fps_ != 0 && audio_queue_.size() > 20 * audio_calc_fps_) || audio_queue_.size() >= 800) && ts_queue_.size() > 10)
    {
        audio_queue_.erase(audio_queue_.begin());
    }

    return kSuccess;
}

int MediaMuxer::OnVideo(Payload& video_payload)
{
    if (video_payload.IsIFrame())
    {
        if (pre_video_key_frame_id_ != 0)
        {
            // 打包ts
            UpdateM3U8();
            ++ts_seq_;
        }

        ++video_key_frame_recv_count_;

        pre_video_key_frame_id_ = video_frame_id_;
        pre_audio_key_frame_id_ = audio_frame_id_;
    }

    video_queue_.insert(make_pair(video_frame_id_, video_payload));

    PacketTs(video_payload);

    ++video_frame_recv_count_;
    ++video_frame_id_;
    ++video_calc_fps_;

    // XXX:可以放到定时器,满了就以后肯定都是满了,不用每次都判断
    if (((video_calc_fps_ != 0 && video_queue_.size() > 20 * video_calc_fps_) || video_queue_.size() >= 800) && ts_queue_.size() > 10)
    {
        if (video_queue_.begin()->second.IsIFrame())
        {
            cout << LMSG << "erase " << ts_queue_.begin()->first << ".ts" << endl;
            ts_queue_.erase(ts_queue_.begin());
        }

        video_queue_.erase(video_queue_.begin());
    }

    return kSuccess;
}

int MediaMuxer::OnMetaData(const string& metadata)
{
    if (metadata_ == metadata)
    {
        cout << LMSG << "metadata no change" << endl;
        return 0;
    }

    metadata_ = metadata;

    return 0;
}

int MediaMuxer::OnVideoHeader(const string& video_header)
{
    if (video_header_.empty() && ! video_header.empty() && ! audio_header_.empty())
    {
        cout << LMSG << "forward_toggle_bit_ " << forward_toggle_bit_ << "->" << true << endl;
        forward_toggle_bit_ = true;
    }

    if (video_header_ == video_header)
    {
        cout << LMSG << "video header no change" << endl;
        return 0;
    }

    video_header_ = video_header;

	BitBuffer bit_buffer(video_header_);

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

int MediaMuxer::OnAudioHeader(const string& audio_header)
{
    if (audio_header_.empty() && ! audio_header.empty() && ! video_header_.empty())
    {
        cout << LMSG << "forward_toggle_bit_ " << forward_toggle_bit_ << "->" << true << endl;
        forward_toggle_bit_ = true;
    }

    if (audio_header_ == audio_header)
    {
        cout << LMSG << "audio header no change" << endl;
        return 0;
    }

    audio_header_ = audio_header;

	uint8_t audio_object_type = 0;
    uint8_t sampling_frequency_index = 0;
    uint8_t channel_config = 0;

    //audio object type:5bit
    audio_object_type = audio_header_[0] & 0xf8;
    audio_object_type >>= 3;

    //sampling frequency index:4bit
    //高3bits
    sampling_frequency_index = audio_header_[0] & 0x07;
    sampling_frequency_index <<= 1;
    //低1bit
    uint8_t tmp = audio_header_[1] & 0x80;
    tmp >>= 7;
    sampling_frequency_index |= tmp;

    cout << LMSG << "sampling_frequency_index:" << (uint16_t)sampling_frequency_index << endl;

    //channel config:4bits
    channel_config = audio_header_[1] & 0x78;
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

    adts_header_[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits 
    adts_header_[6] = 0xfc;
}

vector<Payload> MediaMuxer::GetFastOut()
{
	auto iter_video = video_queue_.find(pre_video_key_frame_id_);
    auto iter_audio = audio_queue_.find(pre_audio_key_frame_id_);

	vector<Payload> fast_out;

    while (iter_audio != audio_queue_.end() || iter_video != video_queue_.end())
    {    
        if (iter_audio == audio_queue_.end())
        {    
            fast_out.push_back(iter_video->second);
            ++iter_video;
        }    
        else if (iter_video == video_queue_.end())
        {    
            fast_out.push_back(iter_audio->second);
            ++iter_audio;
        }    
        else 
        {    
            if (iter_audio->second.GetDts() > iter_video->second.GetDts())
            {    
                fast_out.push_back(iter_video->second);
                ++iter_video;
            }    
            else 
            {    
                fast_out.push_back(iter_audio->second);
                ++iter_audio;
            }    
        }    
    }

    return fast_out;
}

int MediaMuxer::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    if (! video_queue_.empty())
    {
        cout << LMSG << "video queue:" << video_queue_.size() << " [" << video_queue_.begin()->first << "-" << video_queue_.rbegin()->first << "]" << endl;
    }

    if (! audio_queue_.empty())
    {
        cout << LMSG << "audio queue:" << audio_queue_.size() << " [" << audio_queue_.begin()->first << "-" << audio_queue_.rbegin()->first << "]" << endl;
    }

    cout << LMSG << "ts queue:" << ts_queue_.size() << endl;

    if (pre_calc_fps_ms_ == 0)
    {
        pre_calc_fps_ms_ = now_in_ms;
    }
    else
    {
        double duration = (now_in_ms - pre_calc_fps_ms_) / 1000.0;

        cout << LMSG << "[STAT] app:" << app_ 
                     << ",stream:" << stream_name_ 
                     << ",video_fps:" << (video_calc_fps_ / duration)
                     << ",audio_fps:" << (audio_calc_fps_ / duration)
                     << ",interval:" << interval 
                     << ",duration:" << duration
                     << endl;

        pre_calc_fps_ms_ = now_in_ms;

        video_calc_fps_ = 0;
        audio_calc_fps_ = 0;
    }
}
