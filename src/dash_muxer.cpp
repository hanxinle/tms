#include <fcntl.h>
#include <unistd.h>

#include "bit_stream.h"
#include "dash_muxer.h"

#define PRE_SIZE(bs) uint32_t pre_size = bs.SizeInBytes();

#define NEW_SIZE(bs)                       \
  uint32_t new_size = bs.SizeInBytes();    \
  uint32_t box_size = new_size - pre_size; \
  bs.ModifyBytes(pre_size, 4, box_size);

DashMuxer::DashMuxer() { mp4_muxer_.SetSegment(true); }

DashMuxer::~DashMuxer() {}

void DashMuxer::OpenDumpFile(const std::string& file) {
  dump_fd_ = open(file.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
}

void DashMuxer::Dump(const uint8_t* data, const int& len) {
  if (dump_fd_ != -1) {
    int nbytes = write(dump_fd_, data, len);
    UNUSED(nbytes);
  }
}

int DashMuxer::OnAudio(const Payload& payload) {
  audio_samples_.push_back(payload);
  audio_mdat_.append((const char*)payload.GetRawData(), payload.GetRawLen());
  return kSuccess;
}

int DashMuxer::OnVideo(const Payload& payload) {
  if (payload.IsIFrame()) {
    UpdateInitMp4();
    UpdateMpd();
    Flush();
    Reset();
  }

  video_mdat_.append((const char*)payload.GetAllData(), payload.GetAllLen());

  video_samples_.push_back(payload);
  return kSuccess;
}

int DashMuxer::OnMetaData(const std::string& metadata) { return kSuccess; }

int DashMuxer::OnVideoHeader(const std::string& video_header) {
  video_header_ = video_header;
  mp4_muxer_.OnVideoHeader(video_header_);
  return kSuccess;
}

int DashMuxer::OnAudioHeader(const std::string& audio_header) {
  audio_header_ = audio_header;
  mp4_muxer_.OnAudioHeader(audio_header_);
  return kSuccess;
}

void DashMuxer::Flush() {
  if (!video_samples_.empty()) {
    size_t buf_size = video_mdat_.size() + 1024 * 128;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    BitStream bs(buf, buf_size);

    WriteSegmentTypeBox(bs);
    WriteSegmentIndexBox(bs, kVideoPayload);
    WriteMovieFragmentBox(bs, kVideoPayload);
    WriteMediaDataBox(bs, kVideoPayload);

    static int video_count = 0;
    std::ostringstream os;
    os << "dump_video_" << ((video_count++) % 10) << ".m4s";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());

    free(buf);
  }

  if (!audio_samples_.empty()) {
    size_t buf_size = audio_mdat_.size() + 1024 * 128;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    BitStream bs(buf, buf_size);

    WriteSegmentTypeBox(bs);
    WriteSegmentIndexBox(bs, kAudioPayload);
    WriteMovieFragmentBox(bs, kAudioPayload);
    WriteMediaDataBox(bs, kAudioPayload);

    static int audio_count = 0;
    std::ostringstream os;
    os << "dump_audio_" << ((audio_count++) % 10) << ".m4s";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());

    free(buf);
  }
}

void DashMuxer::Reset() {
  video_mdat_.clear();
  audio_mdat_.clear();
  video_samples_.clear();
  audio_samples_.clear();
}

void DashMuxer::UpdateMpd() {
  /*
  <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
  <MPD id="f08e80da-bf1d-4e3d-8899-f0f6155f6efa"
  profiles="urn:mpeg:dash:profile:isoff-main:2011" type="static"
  availabilityStartTime="2015-08-04T09:33:14.000Z"
  publishTime="2015-08-04T10:47:32.000Z"
  mediaPresentationDuration="P0Y0M0DT0H3M30.000S"
  minBufferTime="P0Y0M0DT0H0M1.000S" bitmovin:version="1.6.0"
  xmlns:ns2="http://www.w3.org/1999/xlink" xmlns="urn:mpeg:dash:schema:mpd:2011"
  xmlns:bitmovin="http://www.bitmovin.net/mpd/2015">
      <Period>
          <AdaptationSet mimeType="video/mp4" codecs="avc1.42c00d">
              <SegmentTemplate
  media="../video/$RepresentationID$/dash/segment_$Number$.m4s"
  initialization="../video/$RepresentationID$/dash/init.mp4" duration="100000"
  startNumber="0" timescale="25000"/>
              <Representation id="180_250000" bandwidth="250000" width="320"
  height="180" frameRate="25"/>
              <Representation id="270_400000" bandwidth="400000" width="480"
  height="270" frameRate="25"/>
              <Representation id="360_800000" bandwidth="800000" width="640"
  height="360" frameRate="25"/>
              <Representation id="540_1200000" bandwidth="1200000" width="960"
  height="540" frameRate="25"/>
              <Representation id="720_2400000" bandwidth="2400000" width="1280"
  height="720" frameRate="25"/>
              <Representation id="1080_4800000" bandwidth="4800000" width="1920"
  height="1080" frameRate="25"/>
          </AdaptationSet>
          <AdaptationSet lang="en" mimeType="audio/mp4" codecs="mp4a.40.2"
  bitmovin:label="English stereo">
              <AudioChannelConfiguration
  schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011"
  value="2"/>
              <SegmentTemplate
  media="../audio/$RepresentationID$/dash/segment_$Number$.m4s"
  initialization="../audio/$RepresentationID$/dash/init.mp4" duration="191472"
  startNumber="0" timescale="48000"/>
              <Representation id="1_stereo_128000" bandwidth="128000"
  audioSamplingRate="48000"/>
          </AdaptationSet>
      </Period>
  </MPD>
  */
}

void DashMuxer::UpdateInitMp4() {
  if (!video_samples_.empty()) {
    size_t buf_size = 1024 * 256;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    BitStream bs(buf, buf_size);
    mp4_muxer_.WriteFileTypeBox(bs);
    mp4_muxer_.WriteFreeBox(bs);
    mp4_muxer_.WriteMovieBox(bs, kVideoPayload);

    static int init_video_count = 0;
    std::ostringstream os;
    os << "dump_init_video" << ((init_video_count++) % 10) << ".mp4";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());
    free(buf);
  }

  if (!audio_samples_.empty()) {
    size_t buf_size = 1024 * 256;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    BitStream bs(buf, buf_size);
    mp4_muxer_.WriteFileTypeBox(bs);
    mp4_muxer_.WriteFreeBox(bs);
    mp4_muxer_.WriteMovieBox(bs, kAudioPayload);

    static int init_audio_count = 0;
    std::ostringstream os;
    os << "dump_init_audio" << ((init_audio_count++) % 10) << ".mp4";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());
    free(buf);
  }
}

void DashMuxer::WriteSegmentTypeBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t styp[4] = {'s', 't', 'y', 'p'};
  bs.WriteData(4, styp);

  static uint8_t major_brand[4] = {'m', 's', 'm', 's'};
  bs.WriteData(4, major_brand);

  static uint32_t minor_version = 512;
  bs.WriteBytes(4, minor_version);

  static uint8_t compatible_brands[2][4] = {
      {'m', 's', 'd', 'h'}, {'m', 's', 'i', 'x'},
  };

  for (size_t i = 0; i < 2; ++i) {
    bs.WriteData(4, compatible_brands[i]);
  }

  NEW_SIZE(bs);
}

void DashMuxer::WriteSegmentIndexBox(BitStream& bs,
                                     const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t sidx[4] = {'s', 'i', 'd', 'x'};
  bs.WriteData(4, sidx);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t reference_ID = 1;
  bs.WriteBytes(4, reference_ID);

  uint32_t timescale = 1000;
  bs.WriteBytes(4, timescale);

  if (version == 0) {
    uint32_t earliest_presentation_time = 0;
    bs.WriteBytes(4, earliest_presentation_time);

    uint32_t first_offset = 0;
    bs.WriteBytes(4, first_offset);

    bs.WriteBytes(2, 0);

    uint16_t reference_count = 1;
    bs.WriteBytes(2, reference_count);

    for (int i = 0; i < reference_count; ++i) {
      uint8_t reference_type = 0;
      bs.WriteBits(1, reference_type);

      uint32_t referenced_size = (payload_type == kVideoPayload)
                                     ? video_mdat_.size()
                                     : audio_mdat_.size();
      bs.WriteBits(31, referenced_size);

      uint32_t subsegment_duration = 40;
      bs.WriteBytes(4, subsegment_duration);

      uint8_t starts_with_SAP = 1;
      bs.WriteBits(1, starts_with_SAP);

      uint8_t SAP_type = 1;
      bs.WriteBits(3, SAP_type);

      uint32_t SAP_delta_time = 0;
      bs.WriteBits(28, SAP_delta_time);
    }
  } else {
  }

  NEW_SIZE(bs);
}

void DashMuxer::WriteMovieFragmentBox(BitStream& bs,
                                      const PayloadType& payload_type) {
  PRE_SIZE(bs);

  moof_offset_ = bs.SizeInBytes();

  bs.WriteBytes(4, 0);
  static uint8_t moof[4] = {'m', 'o', 'o', 'f'};
  bs.WriteData(4, moof);

  WriteMovieFragmentHeaderBox(bs);
  WriteTrackFragmentBox(bs, payload_type);

  NEW_SIZE(bs);
}

void DashMuxer::WriteMovieFragmentHeaderBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mfhd[4] = {'m', 'f', 'h', 'd'};
  bs.WriteData(4, mfhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t reference_ID = 0;
  bs.WriteBytes(4, reference_ID);

  uint32_t sequence_number = 0;
  bs.WriteBytes(4, sequence_number);

  NEW_SIZE(bs);
}

void DashMuxer::WriteTrackFragmentBox(BitStream& bs,
                                      const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t traf[4] = {'t', 'r', 'a', 'f'};
  bs.WriteData(4, traf);

  WriteTrackFragmentHeaderBox(bs, payload_type);
  WriteTrackFragmentDecodeTimeBox(bs, payload_type);
  WriteTrackFragmentRunBox(bs, payload_type);

  NEW_SIZE(bs);
}

void DashMuxer::WriteTrackFragmentHeaderBox(BitStream& bs,
                                            const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t tfhd[4] = {'t', 'f', 'h', 'd'};
  bs.WriteData(4, tfhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  // FIXME: why
  uint32_t tf_flags = 0x020000;
  bs.WriteBytes(3, tf_flags);

  uint32_t track_ID = 1;
  bs.WriteBytes(4, track_ID);

  if (tf_flags & 0x000001) {
    uint64_t base_data_offset = 0;
    bs.WriteBytes(8, base_data_offset);
  }

  if (tf_flags & 0x000002) {
    uint32_t sample_description_index = 0;
    bs.WriteBytes(4, sample_description_index);
  }

  if (tf_flags & 0x000008) {
    uint32_t default_sample_duration = 0;
    bs.WriteBytes(4, default_sample_duration);
  }

  if (tf_flags & 0x000010) {
    uint32_t default_sample_size = 0;
    bs.WriteBytes(4, default_sample_size);
  }

  if (tf_flags & 0x000020) {
    uint32_t default_sample_flags = 0;
    bs.WriteBytes(4, default_sample_flags);
  }

  if (tf_flags & 0x01000) {
  }

  if (tf_flags & 0x02000) {
  }

  NEW_SIZE(bs);
}

void DashMuxer::WriteTrackFragmentRunBox(BitStream& bs,
                                         const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t trun[4] = {'t', 'r', 'u', 'n'};
  bs.WriteData(4, trun);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t tr_flags = 0x000001 | 0x000004 | 0x000200 | 0x000800;
  bs.WriteBytes(3, tr_flags);

  std::vector<Payload>& samples =
      (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
  uint32_t sample_count = samples.size();
  bs.WriteBytes(4, sample_count);

  uint32_t pos = bs.SizeInBytes();
  if (tr_flags & 0x000001) {
    int32_t data_offset = 0;
    bs.WriteBytes(4, data_offset);
  }

  if (tr_flags & 0x000004) {
    uint32_t first_sample_flags = 0;
    bs.WriteBytes(4, first_sample_flags);
  }

  for (size_t i = 0; i < sample_count; ++i) {
    if (tr_flags & 0x000100) {
      uint32_t sample_duration = 0;
      bs.WriteBytes(4, sample_duration);
    }

    if (tr_flags & 0x000200) {
      uint32_t sample_size =
          (payload_type == kVideoPayload ? samples[i].GetAllLen()
                                         : samples[i].GetRawLen());
      bs.WriteBytes(4, sample_size);
    }

    if (tr_flags & 0x000400) {
      uint32_t sample_flags = 0;
      bs.WriteBytes(4, sample_flags);
    }

    if (tr_flags & 0x000800) {
      if (version == 0) {
        uint32_t sample_composition_time_offset =
            samples[i].GetPts() - samples[i].GetDts();
        bs.WriteBytes(4, sample_composition_time_offset);
      } else {
        int32_t sample_composition_time_offset =
            samples[i].GetPts() - samples[i].GetDts();
        bs.WriteBytes(4, sample_composition_time_offset);
      }
    }
  }

  bs.ModifyBytes(pos, 4, bs.SizeInBytes() + 8 - moof_offset_);

  NEW_SIZE(bs);
}

void DashMuxer::WriteTrackFragmentDecodeTimeBox(
    BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t tfdt[4] = {'t', 'f', 'd', 't'};
  bs.WriteData(4, tfdt);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  if (version == 1) {
    uint64_t base_media_decode_time = 0;
    bs.WriteBytes(8, base_media_decode_time);
  } else {
    uint32_t base_media_decode_time = 0;
    bs.WriteBytes(4, base_media_decode_time);
  }

  NEW_SIZE(bs);
}

void DashMuxer::WriteMediaDataBox(BitStream& bs,
                                  const PayloadType& payload_type) {
  PRE_SIZE(bs);

  uint32_t size = 0;
  bs.WriteBytes(4, size);
  static uint8_t mdat[4] = {'m', 'd', 'a', 't'};
  bs.WriteData(4, mdat);

  std::string& buf =
      (payload_type == kVideoPayload) ? video_mdat_ : audio_mdat_;
  bs.WriteData(buf.size(), (const uint8_t*)buf.data());

  NEW_SIZE(bs);
}
