#ifndef __MEDIA_MUXER_H__
#define __MEDIA_MUXER_H__

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "crc32.h"
#include "media_struct.h"
#include "ref_ptr.h"
#include "socket_util.h"

class MediaPublisher;

class MediaMuxer {
 public:
  MediaMuxer(MediaPublisher* media_publisher);
  ~MediaMuxer();

  int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval,
                   const uint64_t& count);

  void SetApp(const std::string& app) { app_ = app; }

  void SetStreamName(const std::string& name) { stream_ = name; }

  std::string GetM3U8() { return m3u8_; }

  const std::string& GetTs(const uint64_t& ts) const {
    auto iter = ts_queue_.find(ts);

    if (iter == ts_queue_.end()) {
      return invalid_ts_;
    }

    return iter->second.ts_data;
  }

  const std::string& GetVideoHeader() { return video_header_; }

  bool HasVideoHeader() const { return !video_header_.empty(); }

  const std::string& GetAudioHeader() { return audio_header_; }

  bool HasAudioHeader() const { return !audio_header_.empty(); }

  const std::string& GetMetaData() { return metadata_; }

  bool HasMetaData() const { return !metadata_.empty(); }

  void UpdateM3U8();
  void PacketTs(const Payload& payload);
  std::string& PacketTsPmt();
  std::string& PacketTsPat();

  uint16_t GetAudioContinuityCounter() {
    uint16_t ret = ts_audio_continuity_counter_;

    ++ts_audio_continuity_counter_;

    if (ts_audio_continuity_counter_ == 0x10) {
      ts_audio_continuity_counter_ = 0x00;
    }

    return ret;
  }

  uint16_t GetVideoContinuityCounter() {
    uint16_t ret = ts_video_continuity_counter_;

    ++ts_video_continuity_counter_;

    if (ts_video_continuity_counter_ == 0x10) {
      ts_video_continuity_counter_ = 0x00;
    }

    return ret;
  }

  uint16_t GetPatContinuityCounter() {
    uint16_t ret = ts_pat_continuity_counter_;

    ++ts_pat_continuity_counter_;

    if (ts_pat_continuity_counter_ == 0x10) {
      ts_pat_continuity_counter_ = 0x00;
    }

    return ret;
  }

  uint16_t GetPmtContinuityCounter() {
    uint16_t ret = ts_pmt_continuity_counter_;

    ++ts_pmt_continuity_counter_;

    if (ts_pmt_continuity_counter_ == 0x10) {
      ts_pmt_continuity_counter_ = 0x00;
    }

    return ret;
  }

  int OnAudio(const Payload& payload);
  int OnVideo(const Payload& payload);
  int OnMetaData(const std::string& metadata);
  int OnVideoHeader(const std::string& video_header);
  int OnAudioHeader(const std::string& audio_header);

  std::vector<Payload> GetFastOut();

 private:
  std::string app_;
  std::string stream_;

  std::map<uint64_t, Payload> video_queue_;
  std::map<uint64_t, Payload> audio_queue_;

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

  std::string metadata_;
  std::string audio_header_;
  std::string video_header_;

  uint8_t adts_header_[7];

  std::string vps_;
  std::string sps_;
  std::string pps_;

  // ======== ts ========
  std::string invalid_ts_;

  std::map<uint64_t, TsMedia> ts_queue_;

  std::string m3u8_;
  std::string ts_pat_;
  std::string ts_pmt_;

  uint64_t ts_seq_;
  uint8_t ts_couter_;
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

#endif  // __MEDIA_MUXER_H__
