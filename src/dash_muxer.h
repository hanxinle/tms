#include <string>
#include <vector>

#include "ref_ptr.h"
#include "mp4_muxer.h"

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

 private:
  void Flush();
  void Reset();
  void UpdateMpd();
  void UpdateInitMp4();
  void WriteSegmentTypeBox(BitStream& bs);
  void WriteSegmentIndexBox(BitStream& bs);
  void WriteMovieFragmentBox(BitStream& bs, const PayloadType& payload_type);
  void WriteMovieFragmentHeaderBox(BitStream& bs);
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
  std::vector<Payload> video_samples_;
  std::vector<Payload> audio_samples_;
  std::string video_mdat_;
  std::string audio_mdat_;
  std::string mpd_;
  Mp4Muxer mp4_muxer_;

 private:
  int dump_fd_;
};
