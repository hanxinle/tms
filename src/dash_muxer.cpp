#include <fcntl.h>
#include <unistd.h>

#include "bit_stream.h"
#include "dash_muxer.h"
#include "mp4_muxer.h"

#define MPD_HEADER                                                           \
  "<?xml version=\"1.0\"?>\n"                                                \
  "<MPD\n"                                                                   \
  "    type=\"dynamic\"\n"                                                   \
  "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"                            \
  "    availabilityStartTime=\"%s\"\n"                                       \
  "    publishTime=\"%s\"\n"                                                 \
  "    minimumUpdatePeriod=\"PT%.1fS\"\n"                                    \
  "    minBufferTime=\"PT%.1fS\"\n"                                          \
  "    timeShiftBufferDepth=\"PT%1.fS\"\n"                                   \
  "    profiles=\"urn:hbbtv:dash:profile:isoff-live:2012,"                   \
  "urn:mpeg:dash:profile:isoff-live:2011\"\n"                                \
  "    xmlns:xsi=\"http://www.w3.org/2011/XMLSchema-instance\"\n"            \
  "    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\">\n" \
  "  <Period start=\"PT0S\" id=\"dash\">\n"

#define MPD_SEGMENT_TIMELINE "             <S t=\"%u\" d=\"%d\"/>\n"

#define MPD_VIDEO_HEADER                                          \
  "    <AdaptationSet\n"                                          \
  "        id=\"video_origin\"\n"                                 \
  "        segmentAlignment=\"true\"\n"                           \
  "        maxWidth=\"1920\"\n"                                   \
  "        maxHeight=\"1080\"\n"                                  \
  "        maxFrameRate=\"30\">\n"                                \
  "      <Representation\n"                                       \
  "          id=\"video\"\n"                                      \
  "          mimeType=\"video/mp4\"\n"                            \
  "          codecs=\"avc1.4d0028\"\n"                            \
  "          width=\"1920\"\n"                                    \
  "          height=\"1080\"\n"                                   \
  "          frameRate=\"30\"\n"                                  \
  "          startWithSAP=\"1\"\n"                                \
  "          bandwidth=\"2000000\">\n"                            \
  "        <SegmentTemplate\n"                                    \
  "            timescale=\"1000\"\n"                              \
  "            media=\"$RepresentationID$_$Time$.m4s\"\n"         \
  "            initialization=\"$RepresentationID$_init.mp4\">\n" \
  "          <SegmentTimeline>\n"

#define MPD_VIDEO_TAILER           \
  "          </SegmentTimeline>\n" \
  "        </SegmentTemplate>\n"   \
  "      </Representation>\n"      \
  "    </AdaptationSet>\n"

#define MPD_AUDIO_HEADER                                          \
  "    <AdaptationSet\n"                                          \
  "        id=\"audio_origin\"\n"                                 \
  "        segmentAlignment=\"true\">\n"                          \
  "      <AudioChannelConfiguration\n"                            \
  "          schemeIdUri=\"urn:mpeg:dash:23003:3"                 \
  ":audio_channel_configuration:2011\"\n"                         \
  "          value=\"1\"/>\n"                                     \
  "      <Representation\n"                                       \
  "          id=\"audio\"\n"                                      \
  "          mimeType=\"audio/mp4\"\n"                            \
  "          codecs=\"mp4a.40.2\"\n"                              \
  "          audioSamplingRate=\"44100\"\n"                       \
  "          startWithSAP=\"1\"\n"                                \
  "          bandwidth=\"160000\">\n"                             \
  "        <SegmentTemplate\n"                                    \
  "            timescale=\"1000\"\n"                              \
  "            media=\"$RepresentationID$_$Time$.m4s\"\n"         \
  "            initialization=\"$RepresentationID$_init.mp4\">\n" \
  "          <SegmentTimeline>\n"

#define MPD_AUDIO_TAILER           \
  "          </SegmentTimeline>\n" \
  "        </SegmentTemplate>\n"   \
  "      </Representation>\n"      \
  "    </AdaptationSet>\n"         \
  "  </Period>\n"                  \
  "</MPD>"

#define DASH_CACHE 3
#define DASH_FRAGMENT_DURATION 5000

#define PRE_SIZE(bs) uint32_t pre_size = bs.SizeInBytes();

#define NEW_SIZE(bs)                       \
  uint32_t new_size = bs.SizeInBytes();    \
  uint32_t box_size = new_size - pre_size; \
  bs.ModifyBytes(pre_size, 4, box_size);

DashMuxer::DashMuxer() {
  availability_start_time_utc_str_ = Util::GetNowUTCStr();
  video_sequence_ = 0;
  audio_sequence_ = 0;
}

DashMuxer::~DashMuxer() {}

void DashMuxer::OpenDumpFile(const std::string &file) {
  dump_fd_ = open(file.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
}

void DashMuxer::Dump(const uint8_t *data, const int &len) {
  if (dump_fd_ != -1) {
    int nbytes = write(dump_fd_, data, len);
    UNUSED(nbytes);
  }
}

int DashMuxer::OnAudio(const Payload &payload) {
  audio_samples_.push_back(payload);
  audio_mdat_.append((const char *)payload.GetRawData(), payload.GetRawLen());
  return kSuccess;
}

int DashMuxer::OnVideo(const Payload &payload) {
  video_samples_.push_back(payload);

  if (payload.IsIFrame()) {
    if (Flush()) {
      UpdateInitMp4();
      UpdateMpd();
      Reset();
    }
  }

  video_mdat_.append((const char *)payload.GetAllData(), payload.GetAllLen());
  return kSuccess;
}

int DashMuxer::OnMetaData(const std::string &metadata) { return kSuccess; }

int DashMuxer::OnVideoHeader(const std::string &video_header) {
  video_header_ = video_header;
  return kSuccess;
}

int DashMuxer::OnAudioHeader(const std::string &audio_header) {
  audio_header_ = audio_header;
  return kSuccess;
}

std::string DashMuxer::GetMpd() { return mpd_; }
std::string DashMuxer::GetM4s(const PayloadType &payload_type,
                              const uint64_t &segment_num) {
  std::map<uint64_t, std::string> &m4s =
      ((payload_type == kVideoPayload) ? video_m4s_ : audio_m4s_);

  auto iter = m4s.find(segment_num);
  if (iter == m4s.end() && !m4s.empty()) {
    std::cout << LMSG << "no found "
              << (payload_type == kVideoPayload ? "video " : "audio ")
              << "segment " << segment_num << std::endl;
    std::cout << LMSG << "segment range " << m4s.begin()->first << "-"
              << m4s.rbegin()->first << std::endl;
    return "";
  }

  return iter->second;
}

std::string DashMuxer::GetInitMp4(const PayloadType &payload_type) {
  if (payload_type == kVideoPayload) {
    return video_init_mp4_;
  } else if (payload_type == kAudioPayload) {
    return audio_init_mp4_;
  }

  return "";
}

bool DashMuxer::Flush() {
  int fragment_duration = video_samples_[video_samples_.size() - 1].GetDts() -
                          video_samples_[0].GetDts();
  if (fragment_duration < DASH_FRAGMENT_DURATION) {
    std::cout << "fragment_duration=" << fragment_duration
              << ",DASH_FRAGMENT_DURATION=" << DASH_FRAGMENT_DURATION
              << std::endl;
    return false;
  }

  if (video_samples_.size() > 1) {
    while (video_m4s_.size() > DASH_CACHE * 2) {
      video_m4s_.erase(video_m4s_.begin());
    }

    size_t buf_size = video_mdat_.size() + 1024 * 128;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    BitStream bs(buf, buf_size);

    WriteSegmentTypeBox(bs);
    WriteSegmentIndexBox(bs, kVideoPayload);
    WriteMovieFragmentBox(bs, kVideoPayload);
    WriteMediaDataBox(bs, kVideoPayload);

#if 0
    static int video_count = 0;

    if (true) {
      std::ostringstream os;
      os << "dump_video_" << ((video_count) % 10) << ".m4s";
      OpenDumpFile(os.str());
      Dump(bs.GetData(), bs.SizeInBytes());
    }

    if (true) {
      std::ostringstream os;
      os << "dump_dash_video_" << ((video_count++) % 10) << ".mp4";
      OpenDumpFile(os.str());
      Dump((const uint8_t*)video_init_mp4_.data(), video_init_mp4_.size());
      Dump(bs.GetData(), bs.SizeInBytes());
    }
#endif

    uint32_t video_time = video_samples_[0].GetDts();
    video_m4s_[video_time].assign((const char *)bs.GetData(), bs.SizeInBytes());
    video_mpd_info_[video_sequence_].start_time_ = video_samples_[0].GetDts();
    video_mpd_info_[video_sequence_].duration_ =
        video_samples_[video_samples_.size() - 1].GetDts() -
        video_samples_[0].GetDts();
    std::cout << LMSG << "video segment " << video_time << std::endl;
    ++video_sequence_;

    free(buf);
  }

  if (audio_samples_.size() > 1) {
    while (audio_m4s_.size() > DASH_CACHE * 2) {
      audio_m4s_.erase(audio_m4s_.begin());
    }
    size_t buf_size = audio_mdat_.size() + 1024 * 128;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    BitStream bs(buf, buf_size);

    WriteSegmentTypeBox(bs);
    WriteSegmentIndexBox(bs, kAudioPayload);
    WriteMovieFragmentBox(bs, kAudioPayload);
    WriteMediaDataBox(bs, kAudioPayload);

#if 0
    static int audio_count = 0;
    if (true) {
      std::ostringstream os;
      os << "dump_audio_" << ((audio_count) % 10) << ".m4s";
      OpenDumpFile(os.str());
      Dump(bs.GetData(), bs.SizeInBytes());
    }

    if (true) {
      std::ostringstream os;
      os << "dump_dash_audio_" << ((audio_count++) % 10) << ".mp4";
      OpenDumpFile(os.str());
      Dump((const uint8_t*)audio_init_mp4_.data(), audio_init_mp4_.size());
      Dump(bs.GetData(), bs.SizeInBytes());
    }
#endif

    uint32_t audio_time = audio_samples_[0].GetDts();
    audio_m4s_[audio_time].assign((const char *)bs.GetData(), bs.SizeInBytes());
    audio_mpd_info_[audio_sequence_].start_time_ = audio_samples_[0].GetDts();
    audio_mpd_info_[audio_sequence_].duration_ =
        audio_samples_[audio_samples_.size() - 1].GetDts() -
        audio_samples_[0].GetDts();
    std::cout << LMSG << "audio segment " << audio_time << std::endl;
    ++audio_sequence_;

    free(buf);
  }
  return true;
}

void DashMuxer::Reset() {
  video_mdat_.clear();
  if (!video_samples_.empty()) {
    // 保留最后一个视频帧
    video_samples_.erase(video_samples_.begin(),
                         --video_samples_.rbegin().base());
  }
  if (!audio_samples_.empty()) {
    // 保留最后一个音频帧
    audio_samples_.erase(audio_samples_.begin(),
                         --audio_samples_.rbegin().base());
    audio_mdat_.erase(0, audio_mdat_.size() - audio_samples_[0].GetRawLen());
  }
}

void DashMuxer::UpdateMpd() {
  if (video_sequence_ < DASH_CACHE || audio_sequence_ < DASH_CACHE) {
    return;
  }
  std::ostringstream os;
  int video_duration = 0;
  for (int i = DASH_CACHE; i > 0; --i) {
    video_duration += video_mpd_info_[video_sequence_ - i].duration_;
  }
  int audio_duration = 0;
  for (int i = DASH_CACHE; i > 0; --i) {
    audio_duration += audio_mpd_info_[audio_sequence_ - i].duration_;
  }

  char buf[1024 * 128];

  int nb = snprintf(
      buf, sizeof(buf), MPD_HEADER, availability_start_time_utc_str_.c_str(),
      Util::GetNowUTCStr().c_str(), DASH_FRAGMENT_DURATION / 1000.0,
      (DASH_FRAGMENT_DURATION * 2 / 1000.0),
      (DASH_FRAGMENT_DURATION * (DASH_CACHE * 2) / 1000.0));

  nb += snprintf(buf + nb, sizeof(buf) - nb, MPD_VIDEO_HEADER);

  for (int i = DASH_CACHE; i > 0; --i) {
    nb += snprintf(buf + nb, sizeof(buf) - nb, MPD_SEGMENT_TIMELINE,
                   video_mpd_info_[video_sequence_ - i].start_time_,
                   video_mpd_info_[video_sequence_ - i].duration_);
  }

  nb += snprintf(buf + nb, sizeof(buf) - nb, MPD_VIDEO_TAILER);
  nb += snprintf(buf + nb, sizeof(buf) - nb, MPD_AUDIO_HEADER);

  for (int i = DASH_CACHE; i > 0; --i) {
    nb += snprintf(buf + nb, sizeof(buf) - nb, MPD_SEGMENT_TIMELINE,
                   audio_mpd_info_[audio_sequence_ - i].start_time_,
                   audio_mpd_info_[audio_sequence_ - i].duration_);
  }

  nb += snprintf(buf + nb, sizeof(buf) - nb, MPD_AUDIO_TAILER);

  mpd_.assign(buf, nb);
  std::cout << LMSG << "\n======== MPD ========\n" << mpd_ << "\n" << std::endl;
}

void DashMuxer::UpdateInitMp4() {
  if (video_samples_.size() > 1) {
    size_t buf_size = 1024 * 256;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    BitStream bs(buf, buf_size);
    Mp4Muxer video_mp4_muxer;
    video_mp4_muxer.SetSegment(true);
    video_mp4_muxer.OnVideoHeader(video_header_);
    video_mp4_muxer.WriteFileTypeBox(bs);
    video_mp4_muxer.WriteMovieBox(bs, kVideoPayload);

#if 0
    static int init_video_count = 0;
    std::ostringstream os;
    os << "dump_init_video" << ((init_video_count++) % 10) << ".mp4";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());
#endif

    video_init_mp4_.assign((const char *)bs.GetData(), bs.SizeInBytes());

    free(buf);
  }

  if (audio_samples_.size() > 1) {
    size_t buf_size = 1024 * 256;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    BitStream bs(buf, buf_size);

    Mp4Muxer audio_mp4_muxer;
    audio_mp4_muxer.SetSegment(true);
    audio_mp4_muxer.OnAudioHeader(audio_header_);
    audio_mp4_muxer.WriteFileTypeBox(bs);
    audio_mp4_muxer.WriteMovieBox(bs, kAudioPayload);

#if 0
    static int init_audio_count = 0;
    std::ostringstream os;
    os << "dump_init_audio" << ((init_audio_count++) % 10) << ".mp4";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());
#endif

    audio_init_mp4_.assign((const char *)bs.GetData(), bs.SizeInBytes());

    free(buf);
  }
}

void DashMuxer::WriteSegmentTypeBox(BitStream &bs) {
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

void DashMuxer::WriteSegmentIndexBox(BitStream &bs,
                                     const PayloadType &payload_type) {
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

  std::vector<Payload> &samples =
      (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;

  if (version == 0) {
    uint32_t earliest_presentation_time = samples[0].GetPts();
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

      size_t left_size =
          (payload_type == kAudioPayload)
              ? audio_samples_[audio_samples_.size() - 1].GetRawLen()
              : 0;

      bs.WriteBits(31, referenced_size - left_size);

      uint32_t subsegment_duration = 0;
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

void DashMuxer::WriteMovieFragmentBox(BitStream &bs,
                                      const PayloadType &payload_type) {
  PRE_SIZE(bs);

  moof_offset_ = bs.SizeInBytes();

  bs.WriteBytes(4, 0);
  static uint8_t moof[4] = {'m', 'o', 'o', 'f'};
  bs.WriteData(4, moof);

  WriteMovieFragmentHeaderBox(bs, payload_type);
  WriteTrackFragmentBox(bs, payload_type);

  NEW_SIZE(bs);
}

void DashMuxer::WriteMovieFragmentHeaderBox(BitStream &bs,
                                            const PayloadType &payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mfhd[4] = {'m', 'f', 'h', 'd'};
  bs.WriteData(4, mfhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t &sequence_number =
      payload_type == kVideoPayload ? video_sequence_ : audio_sequence_;
  std::cout << LMSG << ((payload_type == kVideoPayload) ? "video" : "audio")
            << " sequence_number=" << sequence_number << std::endl;
  bs.WriteBytes(4, sequence_number);

  NEW_SIZE(bs);
}

void DashMuxer::WriteTrackFragmentBox(BitStream &bs,
                                      const PayloadType &payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t traf[4] = {'t', 'r', 'a', 'f'};
  bs.WriteData(4, traf);

  WriteTrackFragmentHeaderBox(bs, payload_type);
  WriteTrackFragmentDecodeTimeBox(bs, payload_type);
  WriteTrackFragmentRunBox(bs, payload_type);

  NEW_SIZE(bs);
}

void DashMuxer::WriteTrackFragmentHeaderBox(BitStream &bs,
                                            const PayloadType &payload_type) {
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

void DashMuxer::WriteTrackFragmentRunBox(BitStream &bs,
                                         const PayloadType &payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t trun[4] = {'t', 'r', 'u', 'n'};
  bs.WriteData(4, trun);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t tr_flags = 0x000001 | 0x000004 | 0x000100 | 0x000200 | 0x000800;
  bs.WriteBytes(3, tr_flags);

  std::vector<Payload> &samples =
      (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
  uint32_t sample_count = samples.size() - 1;
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
      uint32_t sample_duration = samples[i + 1].GetDts() - samples[i].GetDts();
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
    BitStream &bs, const PayloadType &payload_type) {
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
    std::vector<Payload> &samples =
        (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
    uint32_t base_media_decode_time = samples[0].GetDts();
    bs.WriteBytes(4, base_media_decode_time);
  }

  NEW_SIZE(bs);
}

void DashMuxer::WriteMediaDataBox(BitStream &bs,
                                  const PayloadType &payload_type) {
  PRE_SIZE(bs);

  uint32_t size = 0;
  bs.WriteBytes(4, size);
  static uint8_t mdat[4] = {'m', 'd', 'a', 't'};
  bs.WriteData(4, mdat);

  std::string &buf =
      (payload_type == kVideoPayload) ? video_mdat_ : audio_mdat_;
  size_t left_size = (payload_type == kAudioPayload)
                         ? audio_samples_[audio_samples_.size() - 1].GetRawLen()
                         : 0;
  bs.WriteData(buf.size() - left_size, (const uint8_t *)buf.data());

  NEW_SIZE(bs);
}
