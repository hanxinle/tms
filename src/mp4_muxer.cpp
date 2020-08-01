#include <fcntl.h>
#include <unistd.h>

#include "bit_stream.h"
#include "mp4_muxer.h"
#include "util.h"

#define PRE_SIZE(bs) uint32_t pre_size = bs.SizeInBytes();

#define NEW_SIZE(bs)                       \
  uint32_t new_size = bs.SizeInBytes();    \
  uint32_t box_size = new_size - pre_size; \
  bs.ModifyBytes(pre_size, 4, box_size);

const int k1M = 1024 * 1024;

Mp4Muxer::Chunk::Chunk() {
  count_ = 0;
  size_ = 0;
  payload_type_ = kUnknownPayload;
  delta_ = 0;
  dts_ = 0;
}

Mp4Muxer::Chunk::Chunk(const Chunk& rhs) { operator=(rhs); }

Mp4Muxer::Chunk& Mp4Muxer::Chunk::operator=(const Chunk& rhs) {
  count_ = rhs.count_;
  size_ = rhs.size_;
  payload_type_ = rhs.payload_type_;
  delta_ = rhs.delta_;
  dts_ = rhs.dts_;

  return *this;
}

Mp4Muxer::Mp4Muxer() : segment_(false), dump_fd_(-1) {}

Mp4Muxer::~Mp4Muxer() {}

void Mp4Muxer::SetSegment(const bool& b) { segment_ = b; }

void Mp4Muxer::OpenDumpFile(const std::string& file) {
  dump_fd_ = open(file.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
}

void Mp4Muxer::Dump(const uint8_t* data, const int& len) {
  if (dump_fd_ != -1) {
    int nbytes = write(dump_fd_, data, len);
    UNUSED(nbytes);
  }
}

int Mp4Muxer::OnAudio(const Payload& payload) {
  audio_samples_.push_back(payload);

  mdat_.append((const char*)payload.GetRawData(), payload.GetRawLen());

  CalChunk(payload.GetDts(), payload.GetRawLen(), kAudioPayload);

  return kSuccess;
}

int Mp4Muxer::OnVideo(const Payload& payload) {
  if (payload.IsIFrame()) {
    if (!chunk_.empty()) {
      std::vector<uint32_t>& chunk_offset =
          (chunk_.back().payload_type_ == kVideoPayload) ? video_chunk_offset_
                                                         : audio_chunk_offset_;
      chunk_offset.push_back(chunk_offset_);
    }

    Flush();
    Reset();
  }

  mdat_.append((const char*)payload.GetAllData(), payload.GetAllLen());

  CalChunk(payload.GetDts(), payload.GetAllLen(), kVideoPayload);

  video_samples_.push_back(payload);

  return kSuccess;
}

void Mp4Muxer::CalChunk(const uint64_t dts, const uint64_t& len,
                        const PayloadType& payload_type) {
  if (!chunk_.empty() && chunk_.back().payload_type_ == payload_type &&
      chunk_.back().size_ + len <= k1M) {
    chunk_.back().size_ += len;
    chunk_.back().count_ += 1;
  } else {
    if (!chunk_.empty()) {
      chunk_.back().delta_ = dts - chunk_.back().dts_;
      std::vector<uint32_t>& chunk_offset =
          (chunk_.back().payload_type_ == kVideoPayload) ? video_chunk_offset_
                                                         : audio_chunk_offset_;

      chunk_offset.push_back(chunk_offset_);
      chunk_offset_ += chunk_.back().size_;
    }

    Chunk chunk;
    chunk.size_ = len;
    chunk.count_ = 1;
    chunk.payload_type_ = payload_type;
    chunk.dts_ = dts;
    chunk_.push_back(chunk);
  }
}

int Mp4Muxer::OnMetaData(const std::string& metadata) { return kSuccess; }

int Mp4Muxer::OnVideoHeader(const std::string& video_header) {
  video_header_ = video_header;

  return kSuccess;
}

int Mp4Muxer::OnAudioHeader(const std::string& audio_header) {
  audio_header_ = audio_header;

  std::cout << LMSG << "audio header=" << Util::Bin2Hex(audio_header_)
            << std::endl;

  return kSuccess;
}

void Mp4Muxer::Flush() {
  if (video_samples_.empty() && audio_samples_.empty()) {
    return;
  }

  size_t buf_size = mdat_.size() + 10 * k1M;
  uint8_t* buf = (uint8_t*)malloc(buf_size);
  BitStream bs(buf, buf_size);
  WriteFileTypeBox(bs);

  if (!segment_) {
    WriteFreeBox(bs);
  }
  WriteMediaDataBox(bs);
  WriteMovieBox(bs);

#if 0
  static int count = 0;
  std::ostringstream os;
  os << "dump_" << ((count++) % 10) << ".mp4";
  OpenDumpFile(os.str());
  Dump(bs.GetData(), bs.SizeInBytes());
#endif

  free(buf);
}

void Mp4Muxer::Reset() {
  chunk_offset_ = 0;
  media_offset_ = 0;
  chunk_.clear();
  mdat_.clear();
  video_chunk_offset_.clear();
  audio_chunk_offset_.clear();
  video_samples_.clear();
  audio_samples_.clear();
}

void Mp4Muxer::WriteFileTypeBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t ftyp[4] = {'f', 't', 'y', 'p'};
  bs.WriteData(4, ftyp);

  if (!segment_) {
    static uint8_t major_brand_isom[4] = {'i', 's', 'o', 'm'};
    bs.WriteData(4, major_brand_isom);
  } else {
    static uint8_t major_brand_iso6[4] = {'i', 's', 'o', '6'};
    bs.WriteData(4, major_brand_iso6);
  }

  static uint32_t minor_version = 1;
  bs.WriteBytes(4, minor_version);

  if (!segment_) {
    static uint8_t compatible_brands[4][4] = {
        {'i', 's', 'o', 'm'},
        {'i', 's', 'o', '2'},
        {'a', 'v', 'c', '1'},
        {'m', 'p', '4', '1'},
    };

    for (size_t i = 0; i < 4; ++i) {
      bs.WriteData(4, compatible_brands[i]);
    }
  } else {
    static uint8_t compatible_brands_segment[3][4] = {
        //{'a', 'v', 'c', '1'}, {'i', 's', 'o', '6'}, {'d', 'a', 's', 'h'},
        {'i', 's', 'o', 'm'},
        {'i', 's', 'o', '6'},
        {'d', 'a', 's', 'h'},
    };

    for (size_t i = 0; i < 3; ++i) {
      bs.WriteData(4, compatible_brands_segment[i]);
    }
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteFreeBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t free[4] = {'f', 'r', 'e', 'e'};
  bs.WriteData(4, free);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMovieBox(BitStream& bs) {
  PRE_SIZE(bs);

  uint32_t size = 0;
  bs.WriteBytes(4, size);
  static uint8_t moov[4] = {'m', 'o', 'o', 'v'};
  bs.WriteData(4, moov);

  WriteMovieHeaderBox(bs);
  WriteTrackBox(bs, kVideoPayload);
  WriteTrackBox(bs, kAudioPayload);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMovieBox(BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  uint32_t size = 0;
  bs.WriteBytes(4, size);
  static uint8_t moov[4] = {'m', 'o', 'o', 'v'};
  bs.WriteData(4, moov);

  WriteMovieHeaderBox(bs);
  WriteMovieExtendsBox(bs);
  WriteTrackBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMediaDataBox(BitStream& bs) {
  PRE_SIZE(bs);

  uint32_t size = 0;
  bs.WriteBytes(4, size);
  static uint8_t mdat[4] = {'m', 'd', 'a', 't'};
  bs.WriteData(4, mdat);

  media_offset_ = bs.SizeInBytes();

  bs.WriteData(mdat_.size(), (const uint8_t*)mdat_.data());

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMovieHeaderBox(BitStream& bs) {
  PRE_SIZE(bs);

  uint32_t size = 0;
  bs.WriteBytes(4, size);
  static uint8_t mvhd[4] = {'m', 'v', 'h', 'd'};
  bs.WriteData(4, mvhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  if (version == 1) {
  } else {
    uint32_t creation_time = segment_ ? 0 : Util::GetNow();
    bs.WriteBytes(4, creation_time);

    uint32_t modification_time = segment_ ? 0 : Util::GetNow();
    bs.WriteBytes(4, modification_time);

    uint32_t timescale = 1000;
    bs.WriteBytes(4, timescale);

    uint32_t video_duration = segment_ ? 0 : 1000;
    uint32_t audio_duration = segment_ ? 0 : 1000;
    if (!video_samples_.empty()) {
      video_duration = (--video_samples_.rbegin().base())->GetDts() -
                       video_samples_.begin()->GetDts();
    }
    if (!audio_samples_.empty()) {
      audio_duration = (--audio_samples_.rbegin().base())->GetDts() -
                       audio_samples_.begin()->GetDts();
    }

    uint32_t duration =
        video_duration > audio_duration ? video_duration : audio_duration;
    ;
    bs.WriteBytes(4, duration);
  }

  uint32_t rate = 0x00010000;
  bs.WriteBytes(4, rate);

  uint16_t volume = 0x0100;
  bs.WriteBytes(2, volume);

  uint16_t reversed = 0;
  bs.WriteBytes(2, reversed);

  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);

  uint32_t matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  for (size_t i = 0; i < 9; ++i) {
    bs.WriteBytes(4, matrix[i]);
  }

  uint32_t pre_defined[6] = {0, 0, 0, 0, 0, 0};
  for (size_t i = 0; i < 6; ++i) {
    bs.WriteBytes(4, pre_defined[i]);
  }

  uint32_t next_track_ID = 1;
  bs.WriteBytes(4, next_track_ID);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteTrackBox(BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t trak[4] = {'t', 'r', 'a', 'k'};
  bs.WriteData(4, trak);

  WriteTrackHeaderBox(bs, payload_type);
  if (!segment_) {
    WriteEditBox(bs, payload_type);
  }
  WriteMediaBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteTrackHeaderBox(BitStream& bs,
                                   const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t tkhd[4] = {'t', 'k', 'h', 'd'};
  bs.WriteData(4, tkhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  // FIXME: any flags?
  uint32_t flags = 3;
  if (segment_) {
    flags = 0x0f;
  }
  bs.WriteBytes(3, flags);

  if (version == 1) {
  } else {
    uint32_t creation_time = segment_ ? 0 : Util::GetNow();
    bs.WriteBytes(4, creation_time);

    uint32_t modification_time = segment_ ? 0 : Util::GetNow();
    bs.WriteBytes(4, modification_time);

    uint32_t track_ID = segment_ ? 1 : (payload_type == kVideoPayload ? 1 : 2);
    bs.WriteBytes(4, track_ID);

    uint32_t reversed = 0;
    bs.WriteBytes(4, reversed);

    if (payload_type == kVideoPayload) {
      uint32_t duration = segment_ ? 0 : 1000;
      if (!video_samples_.empty()) {
        duration = (--video_samples_.rbegin().base())->GetDts() -
                   video_samples_.begin()->GetDts();
      }
      bs.WriteBytes(4, duration);
    } else if (payload_type == kAudioPayload) {
      uint32_t duration = segment_ ? 0 : 1000;
      if (!audio_samples_.empty()) {
        duration = (--audio_samples_.rbegin().base())->GetDts() -
                   audio_samples_.begin()->GetDts();
      }
      bs.WriteBytes(4, duration);
    }
  }

  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);

  uint16_t layer = 0;
  bs.WriteBytes(2, layer);

  uint16_t alternate_group = 0;
  bs.WriteBytes(2, alternate_group);

  uint16_t volume = payload_type == kAudioPayload ? 0x100 : 0;
  bs.WriteBytes(2, volume);

  bs.WriteBytes(2, 0);

  uint32_t matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  for (size_t i = 0; i < 9; ++i) {
    bs.WriteBytes(4, matrix[i]);
  }

  uint32_t width = payload_type == kVideoPayload ? 1920 << 16 : 0;
  bs.WriteBytes(4, width);

  uint32_t height = payload_type == kVideoPayload ? 1080 << 16 : 0;
  bs.WriteBytes(4, height);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteEditBox(BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t edts[4] = {'e', 'd', 't', 's'};
  bs.WriteData(4, edts);

  WriteEditListBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteEditListBox(BitStream& bs,
                                const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t elst[4] = {'e', 'l', 's', 't'};
  bs.WriteData(4, elst);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t entry_count = 1;
  bs.WriteBytes(4, entry_count);

  uint32_t segment_duration = segment_ ? 0 : 1000;
  if (payload_type == kVideoPayload && !video_samples_.empty()) {
    segment_duration = (--video_samples_.rbegin().base())->GetDts() -
                       video_samples_.begin()->GetDts();
  } else if (payload_type == kAudioPayload && !audio_samples_.empty()) {
    segment_duration = (--audio_samples_.rbegin().base())->GetDts() -
                       audio_samples_.begin()->GetDts();
  }
  bs.WriteBytes(4, segment_duration);

  int32_t media_time = 0;
  bs.WriteBytes(4, media_time);

  int16_t media_rate_interger = 1;
  bs.WriteBytes(2, media_rate_interger);

  int16_t media_rate_fraction = 0;
  bs.WriteBytes(2, media_rate_fraction);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMediaBox(BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mdia[4] = {'m', 'd', 'i', 'a'};
  bs.WriteData(4, mdia);

  WriteMediaHeaderBox(bs, payload_type);
  WriteHandlerReferenceBox(bs, payload_type);
  WriteMediaInfoBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMediaHeaderBox(BitStream& bs,
                                   const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mdhd[4] = {'m', 'd', 'h', 'd'};
  bs.WriteData(4, mdhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  if (version == 1) {
  } else {
    uint32_t creation_time = segment_ ? 0 : Util::GetNow();
    bs.WriteBytes(4, creation_time);

    uint32_t modification_time = segment_ ? 0 : Util::GetNow();
    bs.WriteBytes(4, modification_time);

#if 0
    uint32_t timescale = segment_ ? 30000 : 1000;
#else
    uint32_t timescale = 1000;
#endif
    bs.WriteBytes(4, timescale);

    if (payload_type == kVideoPayload) {
      uint32_t duration = segment_ ? 0 : 1000;
      if (!video_samples_.empty()) {
        duration = (--video_samples_.rbegin().base())->GetDts() -
                   video_samples_.begin()->GetDts();
      }
      bs.WriteBytes(4, duration);
    } else if (payload_type == kAudioPayload) {
      uint32_t duration = segment_ ? 0 : 1000;
      if (!audio_samples_.empty()) {
        duration = (--audio_samples_.rbegin().base())->GetDts() -
                   audio_samples_.begin()->GetDts();
      }
      bs.WriteBytes(4, duration);
    }
  }

  uint8_t pad = 0;
  bs.WriteBits(1, pad);

  bs.WriteBits(5, (uint8_t)'e');
  bs.WriteBits(5, (uint8_t)'n');
  bs.WriteBits(5, (uint8_t)'g');

  uint16_t pre_defined = 0;
  bs.WriteBytes(2, pre_defined);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteHandlerReferenceBox(BitStream& bs,
                                        const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t hdlr[4] = {'h', 'd', 'l', 'r'};
  bs.WriteData(4, hdlr);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t pre_defined = 0;
  bs.WriteBytes(4, pre_defined);

  if (payload_type == kVideoPayload) {
    static uint8_t vide[4] = {'v', 'i', 'd', 'e'};
    bs.WriteData(4, vide);
  } else if (payload_type == kAudioPayload) {
    static uint8_t soun[4] = {'s', 'o', 'u', 'n'};
    bs.WriteData(4, soun);
  }

  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);

  if (payload_type == kAudioPayload) { 
    static char audio_handler[] = "AudioHandler";
    bs.WriteData(sizeof(audio_handler), (const uint8_t*)audio_handler);
  } else if (payload_type == kVideoPayload) {
    static char video_handler[] = "VideoHandler";
    bs.WriteData(sizeof(video_handler), (const uint8_t*)video_handler);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMediaInfoBox(BitStream& bs,
                                 const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t minf[4] = {'m', 'i', 'n', 'f'};
  bs.WriteData(4, minf);

  if (payload_type == kVideoPayload) {
    WriteVideoMediaHeaderBox(bs, payload_type);
  } else if (payload_type == kAudioPayload) {
    WriteSoundMediaHeaderBox(bs, payload_type);
  }

  WriteDataInformationBox(bs, payload_type);
  WriteSampleTableBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteVideoMediaHeaderBox(BitStream& bs,
                                        const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t vmhd[4] = {'v', 'm', 'h', 'd'};
  bs.WriteData(4, vmhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 1;
  bs.WriteBytes(3, flags);

  uint16_t graphicsmode = 0;
  bs.WriteBytes(2, graphicsmode);

  uint16_t opcolor[3] = {0, 0, 0};
  for (size_t i = 0; i < 3; ++i) {
    bs.WriteBytes(2, opcolor[i]);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteSoundMediaHeaderBox(BitStream& bs,
                                        const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t smhd[4] = {'s', 'm', 'h', 'd'};
  bs.WriteData(4, smhd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint16_t balance = 0;
  bs.WriteBytes(2, balance);

  uint16_t reserved = 0;
  bs.WriteBytes(2, reserved);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteDataInformationBox(BitStream& bs,
                                       const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t dinf[4] = {'d', 'i', 'n', 'f'};
  bs.WriteData(4, dinf);

  WriteDataReferenceBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteDataReferenceBox(BitStream& bs,
                                     const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t dref[4] = {'d', 'r', 'e', 'f'};
  bs.WriteData(4, dref);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t entry_count = 1;
  bs.WriteBytes(4, entry_count);

  for (size_t i = 0; i < entry_count; ++i) {
    WriteDataEntry(bs, payload_type);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteDataEntry(BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t url[4] = {'u', 'r', 'l', ' '};
  bs.WriteData(4, url);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 1;
  bs.WriteBytes(3, flags);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteSampleTableBox(BitStream& bs,
                                   const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t stbl[4] = {'s', 't', 'b', 'l'};
  bs.WriteData(4, stbl);

  WriteSampleDescriptionBox(bs, payload_type);
  WriteDecodingTimeToSampleBox(bs, payload_type);
  if (payload_type == kVideoPayload) {
    if (!segment_) {
      WriteCompositionTimeToSampleBox(bs, payload_type);
    }
  }
  WriteSampleToChunkBox(bs, payload_type);
  WriteSampleSizeBox(bs, payload_type);
  WriteChunkOffsetBox(bs, payload_type);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteSampleDescriptionBox(BitStream& bs,
                                         const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t stsd[4] = {'s', 't', 's', 'd'};
  bs.WriteData(4, stsd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t entry_count = 1;
  bs.WriteBytes(4, entry_count);

  if (payload_type == kVideoPayload) {
    WriteVisualSampleEntry(bs);
  } else if (payload_type == kAudioPayload) {
    WriteAudioSampleEntry(bs);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteVisualSampleEntry(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t avc1[4] = {'a', 'v', 'c', '1'};
  bs.WriteData(4, avc1);

  bs.WriteBytes(6, (uint64_t)0);

  uint16_t data_reference_index = 1;
  bs.WriteBytes(2, data_reference_index);

  int16_t pre_defined = 0;
  bs.WriteBytes(2, pre_defined);

  bs.WriteBytes(2, 0);

  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);

  uint16_t width = 1920;
  bs.WriteBytes(2, width);

  uint16_t height = 1080;
  bs.WriteBytes(2, height);

  uint32_t horizresolution = 0x00480000;
  bs.WriteBytes(4, horizresolution);

  uint32_t vertrsolution = 0x00480000;
  bs.WriteBytes(4, vertrsolution);

  bs.WriteBytes(4, 0);

  uint16_t frame_count = 1;
  bs.WriteBytes(2, frame_count);

  std::string compressorname(32, '\0');
  bs.WriteData(32, (const uint8_t*)compressorname.data());

  uint16_t depth = 0x0018;
  bs.WriteBytes(2, depth);

  pre_defined = -1;
  bs.WriteBytes(2, pre_defined);

  WriteAVCC(bs);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteAudioSampleEntry(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mp4a[4] = {'m', 'p', '4', 'a'};
  bs.WriteData(4, mp4a);

  bs.WriteBytes(6, 0);

  uint16_t data_reference_index = 1;
  bs.WriteBytes(2, data_reference_index);

  bs.WriteBytes(4, 0);
  bs.WriteBytes(4, 0);

  uint16_t channelcount = 2;
  bs.WriteBytes(2, channelcount);

  uint16_t samplesize = 16;
  bs.WriteBytes(2, samplesize);

  uint16_t pre_defined = 0;
  bs.WriteBytes(2, pre_defined);

  bs.WriteBytes(2, 0);

  uint32_t samplerate = 44100 << 16;
  bs.WriteBytes(4, samplerate);

  WriteEsds(bs);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteEsds(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t esds[4] = {'e', 's', 'd', 's'};
  bs.WriteData(4, esds);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint8_t mp4_es_desc_tag = 0x03;
  bs.WriteBytes(1, mp4_es_desc_tag);
  bs.WriteBytes(1, 0x16);  // FIXME:cal length
  bs.WriteBytes(2, 0);     // Element ID
  bs.WriteBytes(1, 0);     // flags

  uint8_t mp4_dec_config_descr_tag = 0x04;
  bs.WriteBytes(1, mp4_dec_config_descr_tag);
  bs.WriteBytes(1, 0x11);
  bs.WriteBytes(1, 0x40);      // object_type_id(AAC)
  bs.WriteBytes(1, 0x15);      // stream_type
  bs.WriteBytes(3, 0x000300);  // buffer size db
  bs.WriteBytes(4, 192000);    // max bit rate
  bs.WriteBytes(4, 192000);    // avg bit rate

  uint8_t mp4_desc_specific_desc_tag = 0x05;
  bs.WriteBytes(1, mp4_desc_specific_desc_tag);
  bs.WriteBytes(1, 0x02);
  bs.WriteData(2, (const uint8_t*)audio_header_.data());

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteAVCC(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t avcc[4] = {'a', 'v', 'c', 'C'};
  bs.WriteData(4, avcc);

  bs.WriteData(video_header_.size(), (const uint8_t*)video_header_.data());

  // WritePixelAspectRatioBox(bs);

  NEW_SIZE(bs);
}

void Mp4Muxer::WritePixelAspectRatioBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t pasp[4] = {'p', 'a', 's', 'p'};
  bs.WriteData(4, pasp);

  uint32_t h_spacing = 1;
  bs.WriteBytes(4, h_spacing);

  uint32_t v_spacing = 1;
  bs.WriteBytes(4, v_spacing);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteDecodingTimeToSampleBox(BitStream& bs,
                                            const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t stts[4] = {'s', 't', 't', 's'};
  bs.WriteData(4, stts);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  std::vector<Payload>& samples =
      (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;

  uint32_t pos = bs.SizeInBytes();
  uint32_t entry_count = samples.size();
  bs.WriteBytes(4, entry_count);

  uint32_t pre_sample_time = samples.empty() ? 0 : samples[0].GetDts();
  uint32_t pre_sample_delta = 0;
  uint32_t sample_count = 1;
  for (size_t i = 0; i < samples.size(); ++i) {
    uint32_t sample_delta = samples[i].GetDts() - pre_sample_time;
    if (i > 0 && sample_delta == pre_sample_delta) {
      --entry_count;
      ++sample_count;
      continue;
    }

    bs.WriteBytes(4, sample_count);
    bs.WriteBytes(4, sample_delta);

    pre_sample_time = samples[i].GetDts();
    pre_sample_delta = sample_delta;
    sample_count = 1;
  }

  bs.ModifyBytes(pos, 4, entry_count);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteCompositionTimeToSampleBox(
    BitStream& bs, const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t ctts[4] = {'c', 't', 't', 's'};
  bs.WriteData(4, ctts);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  std::vector<Payload>& samples = video_samples_;
  uint32_t entry_count = samples.size();
  bs.WriteBytes(4, entry_count);

  for (size_t i = 0; i < samples.size(); ++i) {
    uint32_t sample_count = 1;
    bs.WriteBytes(4, sample_count);

    uint32_t sample_offset = samples[i].GetPts() - samples[i].GetDts();
    bs.WriteBytes(4, sample_offset);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteSampleToChunkBox(BitStream& bs,
                                     const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t stsc[4] = {'s', 't', 's', 'c'};
  bs.WriteData(4, stsc);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

#if defined(ONE_SAMPLE_PER_CHUNK)
  uint32_t entry_count = 1;
  bs.WriteBytes(4, entry_count);
  {
    uint32_t first_chunk = 1;
    bs.WriteBytes(4, first_chunk);

    uint32_t sample_per_chunk = 1;
    bs.WriteBytes(4, sample_per_chunk);

    uint32_t sample_description_index = 1;
    bs.WriteBytes(4, sample_description_index);
  }
#else
  std::vector<Chunk> tmp;
  for (const auto& item : chunk_) {
    if (item.payload_type_ == payload_type) {
      tmp.push_back(item);
    }
  }

  uint32_t entry_count = tmp.size();
  uint32_t pos = bs.SizeInBytes();
  bs.WriteBytes(4, entry_count);

  for (size_t i = 0; i < tmp.size(); ++i) {
    if (i > 0 && tmp[i].count_ == tmp[i - 1].count_) {
      --entry_count;
      continue;
    }

    uint32_t first_chunk = i + 1;
    bs.WriteBytes(4, first_chunk);

    uint32_t sample_per_chunk = tmp[i].count_;
    bs.WriteBytes(4, sample_per_chunk);

    uint32_t sample_description_index = 1;
    bs.WriteBytes(4, sample_description_index);
  }

  bs.ModifyBytes(pos, 4, entry_count);
#endif

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteChunkOffsetBox(BitStream& bs,
                                   const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t stco[4] = {'s', 't', 'c', 'o'};
  bs.WriteData(4, stco);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  std::vector<uint32_t>& chunk_offset = (payload_type == kVideoPayload)
                                            ? video_chunk_offset_
                                            : audio_chunk_offset_;
  uint32_t entry_count = chunk_offset.size();
  bs.WriteBytes(4, entry_count);

  for (size_t i = 0; i < chunk_offset.size(); ++i) {
    bs.WriteBytes(4, chunk_offset[i] + media_offset_);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteSampleSizeBox(BitStream& bs,
                                  const PayloadType& payload_type) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t stsz[4] = {'s', 't', 's', 'z'};
  bs.WriteData(4, stsz);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t sample_size = 0;
  bs.WriteBytes(4, sample_size);

  std::vector<Payload>& samples =
      (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
  uint32_t sample_count = samples.size();
  bs.WriteBytes(4, sample_count);

  for (size_t i = 0; i < samples.size(); ++i) {
    if (payload_type == kVideoPayload) {
      bs.WriteBytes(4, samples[i].GetAllLen());
    } else {
      bs.WriteBytes(4, samples[i].GetRawLen());
    }
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMovieExtendsBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mvex[4] = {'m', 'v', 'e', 'x'};
  bs.WriteData(4, mvex);

  if (!segment_) {
    WriteMovieExtendsHeaderBox(bs);
  }
  WriteTrackExtendsBox(bs);

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteMovieExtendsHeaderBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t mehd[4] = {'m', 'e', 'h', 'd'};
  bs.WriteData(4, mehd);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  if (version == 1) {
    uint64_t fragment_duration = 20;
    bs.WriteBytes(8, fragment_duration);
  } else {
    uint32_t fragment_duration = 20;
    bs.WriteBytes(4, fragment_duration);
  }

  NEW_SIZE(bs);
}

void Mp4Muxer::WriteTrackExtendsBox(BitStream& bs) {
  PRE_SIZE(bs);

  bs.WriteBytes(4, 0);
  static uint8_t trex[4] = {'t', 'r', 'e', 'x'};
  bs.WriteData(4, trex);

  uint8_t version = 0;
  bs.WriteBytes(1, version);

  uint32_t flags = 0;
  bs.WriteBytes(3, flags);

  uint32_t track_ID = 1;
  bs.WriteBytes(4, track_ID);

  uint32_t default_sample_description_index = 1;
  bs.WriteBytes(4, default_sample_description_index);

  uint32_t default_sample_duration = segment_ ? 0 : 1000;
  bs.WriteBytes(4, default_sample_duration);

  uint32_t default_sample_size = 0;
  bs.WriteBytes(4, default_sample_size);

  uint32_t default_sample_flags = 0;
  bs.WriteBytes(4, default_sample_flags);

  NEW_SIZE(bs);
}
