#include <string>
#include <vector>

#include "mp4_muxer.h"
#include "ref_ptr.h"

class BitStream;

class DashMuxer {
 public:
  DashMuxer();
  ~DashMuxer();

 public:
  void OpenDumpFile(const std::string& file);
  void Dump(const uint8_t* data, const int& len);

  int OnAudio(const Payload& payload);
  int OnVideo(const Payload& payload);
  void CalChunk(const uint64_t dts, const uint64_t& len,
                const PayloadType& payload_type);
  int OnMetaData(const std::string& metadata);
  int OnVideoHeader(const std::string& video_header);
  int OnAudioHeader(const std::string& audio_header);

  std::string GetMpd();
  std::string GetM4s(const PayloadType& payload_type, const uint64_t& segment_num);
  std::string GetInitMp4(const PayloadType& payload_type);

 private:
  void Flush();
  void Reset();
  void UpdateMpd();
  void UpdateInitMp4();
  void WriteSegmentTypeBox(BitStream& bs);
  void WriteSegmentIndexBox(BitStream& bs, const PayloadType& payload_type);
  void WriteMovieFragmentBox(BitStream& bs, const PayloadType& payload_type);
  void WriteMovieFragmentHeaderBox(BitStream& bs, const PayloadType& payload_type);
  void WriteTrackFragmentBox(BitStream& bs, const PayloadType& payload_type);
  void WriteTrackFragmentHeaderBox(BitStream& bs,
                                   const PayloadType& payload_type);
  void WriteTrackFragmentRunBox(BitStream& bs, const PayloadType& payload_type);
  void WriteTrackFragmentDecodeTimeBox(BitStream& bs,
                                       const PayloadType& payload_type);
  void WriteMediaDataBox(BitStream& bs, const PayloadType& payload_type);

 private:
  std::string video_header_;
  std::string audio_header_;

 private:
  uint32_t moof_offset_;

 private:
  std::vector<Payload> video_samples_;
  std::vector<Payload> audio_samples_;
  std::string video_mdat_;
  std::string audio_mdat_;
  uint32_t video_sequence_;
  uint32_t audio_sequence_;
  Mp4Muxer mp4_muxer_;
 private:
  std::map<uint64_t, std::string> video_m4s_;
  std::map<uint64_t, std::string> audio_m4s_;
  std::string video_init_mp4_;
  std::string audio_init_mp4_;
  std::string mpd_;

 private:
  int dump_fd_;
};
