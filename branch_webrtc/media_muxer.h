#ifndef __MEDIA_MUXER_H__
#define __MEDIA_MUXER_H__

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <sstream>
#include <set>
#include <vector>

#include "crc32.h"
#include "media_struct.h"
#include "ref_ptr.h"
#include "socket_util.h"
#include "trace_tool.h"

using std::map;
using std::ostringstream;
using std::string;
using std::set;
using std::vector;

class MediaPublisher;

class MediaMuxer
{
public:
    MediaMuxer(MediaPublisher* media_publisher);
    ~MediaMuxer();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    void SetApp(const string& app)
    {
        app_ = app;
    }

    void SetStreamName(const string& name)
    {
        stream_ = name;
    }

    string GetM3U8()
    {
        return m3u8_;
    }

    const string& GetTs(const uint64_t& ts) const
    {
        auto iter = ts_queue_.find(ts);

        if (iter == ts_queue_.end())
        {
            return invalid_ts_;
        }

        return iter->second.ts_data;
    }

    const string& GetVideoHeader()
    {
        return video_header_;
    }

    bool HasVideoHeader() const
    {
        return ! video_header_.empty();
    }

    const string& GetAudioHeader()
    {
        return audio_header_;
    }

    bool HasAudioHeader() const
    {
        return ! audio_header_.empty();
    }

    const string& GetMetaData()
    {
        return metadata_;
    }

    bool HasMetaData() const
    {
        return ! metadata_.empty();
    }

    void UpdateM3U8();
    void PacketTs(const Payload& payload);
    string& PacketTsPmt();
    string& PacketTsPat();

    uint16_t GetAudioContinuityCounter()
    {
        uint16_t ret = ts_audio_continuity_counter_;

        ++ts_audio_continuity_counter_;

        if (ts_audio_continuity_counter_ == 0x10)
        {
            ts_audio_continuity_counter_ = 0x00;
        }

        return ret;
    }

    uint16_t GetVideoContinuityCounter()
    {
        uint16_t ret = ts_video_continuity_counter_;

        ++ts_video_continuity_counter_;

        if (ts_video_continuity_counter_ == 0x10)
        {
            ts_video_continuity_counter_ = 0x00;
        }

        return ret;
    }

    uint16_t GetPatContinuityCounter()
    {
        uint16_t ret = ts_pat_continuity_counter_;

        ++ts_pat_continuity_counter_;

        if (ts_pat_continuity_counter_ == 0x10)
        {
            ts_pat_continuity_counter_ = 0x00;
        }

        return ret;
    }

    uint16_t GetPmtContinuityCounter()
    {
        uint16_t ret = ts_pmt_continuity_counter_;

        ++ts_pmt_continuity_counter_;

        if (ts_pmt_continuity_counter_ == 0x10)
        {
            ts_pmt_continuity_counter_ = 0x00;
        }

        return ret;
    }

    bool GetForwardToggleBit()
    {
        bool ret = forward_toggle_bit_;

        if (forward_toggle_bit_)
        {
            cout << LMSG << "forward_toggle_bit_ " << forward_toggle_bit_ << "->" << false << endl;
        }

        forward_toggle_bit_ = false;

        return ret;
    }

    int OnAudio(Payload& payload);
    int OnVideo(Payload& payload);
    int OnMetaData(const string& metadata);
    int OnVideoHeader(const string& video_header);
    int OnAudioHeader(const string& audio_header);

    vector<Payload> GetFastOut();

private:
    string app_;
    string stream_;

    map<uint64_t, Payload> video_queue_;
    map<uint64_t, Payload> audio_queue_;

    uint64_t video_frame_id_;
    uint64_t audio_frame_id_;

    uint64_t video_frame_recv_count_;
    uint64_t audio_frame_recv_count_;

    uint64_t video_key_frame_recv_count_;

    uint64_t pre_video_key_frame_id_;
    uint64_t pre_audio_key_frame_id_;

    uint32_t video_calc_fps_;
    uint32_t audio_calc_fps_;

    uint64_t pre_calc_fps_ms_;

    string metadata_;
    string audio_header_;
    string video_header_;

    uint8_t adts_header_[7];

    string vps_;
    string sps_;
    string pps_;

    bool forward_toggle_bit_;

    // ======== ts ========
    string invalid_ts_;

    map<uint64_t, TsMedia> ts_queue_;
    string  srt_send_buf_;

    string m3u8_;
    string ts_pat_;
    string ts_pmt_;

    uint64_t ts_seq_;
    uint8_t  ts_couter_;
    uint32_t ts_video_pid_;
    uint32_t ts_audio_pid_;
    uint32_t ts_pmt_pid_;

    uint8_t ts_pat_continuity_counter_;
    uint8_t ts_pmt_continuity_counter_;
    uint8_t ts_audio_continuity_counter_;
    uint8_t ts_video_continuity_counter_;

    CRC32 crc_32_;

    MediaPublisher* media_publisher_;
};

#endif // __MEDIA_MUXER_H__
