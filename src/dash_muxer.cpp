#include <fcntl.h>
#include <unistd.h>

#include "bit_stream.h"
#include "dash_muxer.h"
#include "util.h"

#define CUR_SIZE(bs) uint32_t cur_size = bs.SizeInBytes(); \
    uint8_t* cur_data = bs.GetData() + cur_size;

#define NEW_SIZE(bs) uint32_t new_size = bs.SizeInBytes(); \
    std::cout << LMSG << "cur_size=" << cur_size << ",new_size=" << new_size << std::endl; \
    uint32_t box_size = new_size - cur_size; \
    cur_data[0] = uint8_t((box_size >> 24) & 0xFF); \
    cur_data[1] = uint8_t((box_size >> 16) & 0xFF); \
    cur_data[2] = uint8_t((box_size >> 8) & 0xFF); \
    cur_data[3] = uint8_t((box_size) & 0xFF);

const int k1M = 1024*1024;

DashMuxer::Chunk::Chunk()
{
    count_ = 0;
    size_ = 0;
    payload_type_ = kUnknownPayload;
    delta_ = 0;
    dts_ = 0;
}

DashMuxer::Chunk::Chunk(const Chunk& rhs)
{
    operator=(rhs);
}

DashMuxer::Chunk& DashMuxer::Chunk::operator=(const Chunk& rhs)
{
    count_ = rhs.count_;
    size_ = rhs.size_;
    payload_type_ = rhs.payload_type_;
    delta_ = rhs.delta_;
    dts_ = rhs.dts_;

    return *this;
}

DashMuxer::DashMuxer()
	: dump_fd_(-1)
{
}

DashMuxer::~DashMuxer()
{
}

void DashMuxer::OpenDumpFile(const std::string& file)
{
    dump_fd_ = open(file.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0664);
}

void DashMuxer::Dump(const uint8_t* data, const int& len)
{
    if (dump_fd_ != -1)
    {
        int nbytes = write(dump_fd_, data, len);
        UNUSED(nbytes);
    }
}

int DashMuxer::OnAudio(const Payload& payload)
{
    audio_samples_.push_back(payload);

    mdat_.append((const char*)payload.GetRawData(), payload.GetRawLen());

    CalChunk(payload.GetDts(), payload.GetRawLen(), kAudioPayload);

    return kSuccess;
}

int DashMuxer::OnVideo(const Payload& payload)
{
    if (payload.IsIFrame())
    {
        if (! chunk_.empty())
        {
            std::vector<uint32_t>& chunk_offset = (chunk_.back().payload_type_ == kVideoPayload) ? video_chunk_offset_ : audio_chunk_offset_;
            chunk_offset.push_back(chunk_offset_);
        }

        Flush();
    }

    mdat_.append((const char*)payload.GetAllData(), payload.GetAllLen());

    CalChunk(payload.GetDts(), payload.GetAllLen(), kVideoPayload);

    video_samples_.push_back(payload);

    return kSuccess;
}

void DashMuxer::CalChunk(const uint64_t dts, const uint64_t& len, const PayloadType& payload_type)
{
    if (! chunk_.empty() && chunk_.back().payload_type_ == payload_type && chunk_.back().size_ + len <= k1M)
    {
        chunk_.back().size_ += len;
        chunk_.back().count_ += 1;
    }
    else
    {
        if (! chunk_.empty())
        {
            chunk_.back().delta_ = dts - chunk_.back().dts_;
            std::vector<uint32_t>& chunk_offset = (chunk_.back().payload_type_ == kVideoPayload) ? video_chunk_offset_ : audio_chunk_offset_;

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

int DashMuxer::OnMetaData(const std::string& metadata)
{
    return kSuccess;
}

int DashMuxer::OnVideoHeader(const std::string& video_header)
{
    video_header_ = video_header;

    return kSuccess;
}

int DashMuxer::OnAudioHeader(const std::string& audio_header)
{
    audio_header_ = audio_header;

    std::cout << LMSG << "audio header=" << Util::Bin2Hex(audio_header_) << std::endl;

    return kSuccess;
}

void DashMuxer::Flush()
{
    if (video_samples_.empty() && audio_samples_.empty())
    {
        return;
    }

    size_t buf_size = 20 * 1024 * 1024;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    BitStream bs(buf, buf_size);
    WriteFileTypeBox(bs);
    WriteFreeBox(bs);
    WriteMediaDataBox(bs);
    WriteMovieBox(bs);

    static int count = 0;
    std::ostringstream os;
    os << "dump_" << count++ << ".mp4";
    OpenDumpFile(os.str());
    Dump(bs.GetData(), bs.SizeInBytes());

    free(buf);

    Reset();
}

void DashMuxer::Reset()
{
    chunk_offset_ = 0;
    media_offset_ = 0;
    chunk_.clear();
    mdat_.clear();
    video_chunk_offset_.clear();
    audio_chunk_offset_.clear();
    video_samples_.clear();
    audio_samples_.clear();
}

void DashMuxer::WriteFileTypeBox(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t ftyp[4] = {'f', 't', 'y', 'p'};
    bs.WriteData(4, ftyp);

    static uint8_t major_brand[4] = {'i', 's', 'o', 'm'};
    bs.WriteData(4, major_brand);

    static uint32_t minor_version = 512;
    bs.WriteBytes(4, minor_version);

    static uint8_t compatible_brands[4][4] = 
    {
        {'i', 's', 'o', 'm'},
        {'i', 's', 'o', '2'},
        {'a', 'v', 'c', '1'},
        {'m', 'p', '4', '1'},
    };

    for (size_t i = 0; i < 4; ++i)
    {
        bs.WriteData(4, compatible_brands[i]);
    }

    NEW_SIZE(bs);
}

void DashMuxer::WriteFreeBox(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t free[4] = {'f', 'r', 'e', 'e'};
    bs.WriteData(4, free);

    NEW_SIZE(bs);
}

void DashMuxer::WriteMovieBox(BitStream& bs)
{
    CUR_SIZE(bs);

    uint32_t size = 0;
    bs.WriteBytes(4, size);
    static uint8_t moov[4] = {'m', 'o', 'o', 'v'};
    bs.WriteData(4, moov);

    WriteMovieHeaderBox(bs);
    WriteTrackBox(bs, kVideoPayload);
    WriteTrackBox(bs, kAudioPayload);

    NEW_SIZE(bs);
}

void DashMuxer::WriteMediaDataBox(BitStream& bs)
{
    CUR_SIZE(bs);

    uint32_t size = 0;
    bs.WriteBytes(4, size);
    static uint8_t mdat[4] = {'m', 'd', 'a', 't'};
    bs.WriteData(4, mdat);

    media_offset_ = bs.SizeInBytes();

#if 0
    chunk_offset_ = bs.SizeInBytes();

    auto iter_v = video_samples_.begin();
    auto iter_a = audio_samples_.begin();

    while (iter_v != video_samples_.end() || iter_a != audio_samples_.end())
    {
        if (iter_v == video_samples_.end())
        {
            bs.WriteData(iter_a->GetRawLen(), iter_a->GetRawData());
            audio_chunk_offset_.push_back(chunk_offset_);
            chunk_offset_ += iter_a->GetRawLen();
            ++iter_a;
        }
        else if (iter_a == audio_samples_.end())
        {
            bs.WriteData(iter_v->GetAllLen(), iter_v->GetAllData());
            video_chunk_offset_.push_back(chunk_offset_);
            chunk_offset_ += iter_v->GetAllLen();
            ++iter_v;
        }
        else
        {
            if (iter_a->GetDts() <= iter_v->GetDts())
            {
                bs.WriteData(iter_a->GetRawLen(), iter_a->GetRawData());
                audio_chunk_offset_.push_back(chunk_offset_);
                chunk_offset_ += iter_a->GetRawLen();
                ++iter_a;
            }
            else
            {
                bs.WriteData(iter_v->GetAllLen(), iter_v->GetAllData());
                video_chunk_offset_.push_back(chunk_offset_);
                chunk_offset_ += iter_v->GetAllLen();
                ++iter_v;
            }
        }
    }
#else
    bs.WriteData(mdat_.size(), (const uint8_t*)mdat_.data());
#endif

    NEW_SIZE(bs);
}

void DashMuxer::WriteMovieHeaderBox(BitStream& bs)
{
    CUR_SIZE(bs);

    uint32_t size = 0;
    bs.WriteBytes(4, size);
    static uint8_t mvhd[4] = {'m', 'v', 'h', 'd'};
    bs.WriteData(4, mvhd);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    if (version == 1)
    {
    }
    else
    {
        uint32_t creation_time = Util::GetNow();
        bs.WriteBytes(4, creation_time);

        uint32_t modification_time = Util::GetNow();
        bs.WriteBytes(4, modification_time);

        uint32_t timescale = 1000;
        bs.WriteBytes(4, timescale);

        uint32_t video_duration = 1000;
        uint32_t audio_duration = 1000;
        if (! video_samples_.empty())
        {
            video_duration = (--video_samples_.rbegin().base())->GetDts() - video_samples_.begin()->GetDts();
        }
        if (! audio_samples_.empty())
        {
            audio_duration = (--audio_samples_.rbegin().base())->GetDts() - audio_samples_.begin()->GetDts();
        }

        uint32_t duration = video_duration > audio_duration ? video_duration : audio_duration;;
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

    uint32_t matrix[9] = { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };;
    for (size_t i = 0; i < 9; ++i)
    {
        bs.WriteBytes(4, matrix[i]);
    }

    uint32_t pre_defined[6] = {0, 0, 0, 0, 0, 0};
    for (size_t i = 0; i < 6; ++i)
    {
        bs.WriteBytes(4, pre_defined[i]);
    }

    uint32_t next_track_ID = 1;
    bs.WriteBytes(4, next_track_ID);

    NEW_SIZE(bs);
}

void DashMuxer::WriteTrackBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t trak[4] = {'t', 'r', 'a', 'k'};
    bs.WriteData(4, trak);

    WriteTrackHeaderBox(bs, payload_type);
    WriteEditBox(bs, payload_type);
    WriteMediaBox(bs, payload_type);

    NEW_SIZE(bs);
}

void DashMuxer::WriteTrackHeaderBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t tkhd[4] = {'t', 'k', 'h', 'd'};
    bs.WriteData(4, tkhd);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    // FIXME: any flags?
    uint32_t flags = 3;
    bs.WriteBytes(3, flags);

    if (version == 1)
    {
    }
    else
    {
        uint32_t creation_time = Util::GetNow();
        bs.WriteBytes(4, creation_time);

        uint32_t modification_time = Util::GetNow();
        bs.WriteBytes(4, modification_time);

        uint32_t track_ID = 1;
        if (payload_type == kAudioPayload)
        {
            track_ID = 2;
        }
        bs.WriteBytes(4, track_ID);

        uint32_t reversed = 0;
        bs.WriteBytes(4, reversed);

        if (payload_type == kVideoPayload)
        {
            uint32_t duration = 1000;
            if (! video_samples_.empty())
            {
                duration = (--video_samples_.rbegin().base())->GetDts() - video_samples_.begin()->GetDts();
            }
            bs.WriteBytes(4, duration);
        }
        else if (payload_type == kAudioPayload)
        {
            uint32_t duration = 1000;
            if (! audio_samples_.empty())
            {
                duration = (--audio_samples_.rbegin().base())->GetDts() - audio_samples_.begin()->GetDts();
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

    uint32_t matrix[9] = { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };
    for (size_t i = 0; i < 9; ++i)
    {
        bs.WriteBytes(4, matrix[i]);
    }

    uint32_t width = payload_type == kVideoPayload ? 1920 << 16 : 0;
    bs.WriteBytes(4, width);

    uint32_t height = payload_type == kVideoPayload ? 1080 << 16 : 0;
    bs.WriteBytes(4, height);

    NEW_SIZE(bs);
}

void DashMuxer::WriteEditBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t edts[4] = {'e', 'd', 't', 's'};
    bs.WriteData(4, edts);

    WriteEditListBox(bs, payload_type);

    NEW_SIZE(bs);
}

void DashMuxer::WriteEditListBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t elst[4] = {'e', 'l', 's', 't'};
    bs.WriteData(4, elst);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    uint32_t entry_count = 1;
    bs.WriteBytes(4, entry_count);

    uint32_t segment_duration = 1000;
    if (payload_type == kVideoPayload && ! video_samples_.empty())
    {
        segment_duration = (--video_samples_.rbegin().base())->GetDts() - video_samples_.begin()->GetDts();
    }
    else if (payload_type == kAudioPayload && ! audio_samples_.empty())
    {
        segment_duration = (--audio_samples_.rbegin().base())->GetDts() - audio_samples_.begin()->GetDts();
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

void DashMuxer::WriteMediaBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t mdia[4] = {'m', 'd', 'i', 'a'};
    bs.WriteData(4, mdia);

    WriteMediaHeaderBox(bs, payload_type);
    WriteHandlerReferenceBox(bs, payload_type);
    WriteMediaInfoBox(bs, payload_type);

    NEW_SIZE(bs);
}

void DashMuxer::WriteMediaHeaderBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t mdhd[4] = {'m', 'd', 'h', 'd'};
    bs.WriteData(4, mdhd);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    if (version == 1)
    {
    }
    else
    {
        uint32_t creation_time = Util::GetNow();
        bs.WriteBytes(4, creation_time);

        uint32_t modification_time = Util::GetNow();
        bs.WriteBytes(4, modification_time);

        uint32_t timescale = 1000;
        bs.WriteBytes(4, timescale);

        if (payload_type == kVideoPayload)
        {
            uint32_t duration = 1000;
            if (! video_samples_.empty())
            {
                duration = (--video_samples_.rbegin().base())->GetDts() - video_samples_.begin()->GetDts();
            }
            bs.WriteBytes(4, duration);
        }
        else if (payload_type == kAudioPayload)
        {
            uint32_t duration = 1000;
            if (! audio_samples_.empty())
            {
                duration = (--audio_samples_.rbegin().base())->GetDts() - audio_samples_.begin()->GetDts();
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

void DashMuxer::WriteHandlerReferenceBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t hdlr[4] = {'h', 'd', 'l', 'r'};
    bs.WriteData(4, hdlr);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    uint32_t pre_defined = 0;
    bs.WriteBytes(4, pre_defined);

    if (payload_type == kVideoPayload)
    {
        static uint8_t vide[4] = {'v', 'i', 'd', 'e'};
        bs.WriteData(4, vide);
    }
    else if (payload_type == kAudioPayload)
    {
        static uint8_t soun[4] = {'s', 'o', 'u', 'n'};
        bs.WriteData(4, soun);
    }

    bs.WriteBytes(4, 0);
    bs.WriteBytes(4, 0);
    bs.WriteBytes(4, 0);

    std::string debug = "VideoHandler#";
    if (payload_type == kAudioPayload)
    {
        debug = "AudioHandler#";
    }
    bs.WriteData(debug.size(), (const uint8_t*)debug.data());

    NEW_SIZE(bs);
}

void DashMuxer::WriteMediaInfoBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t minf[4] = {'m', 'i', 'n', 'f'};
    bs.WriteData(4, minf);

    if (payload_type == kVideoPayload)
    {
        WriteVideoMediaHeaderBox(bs, payload_type);
    }
    else if (payload_type == kAudioPayload)
    {
        WriteSoundMediaHeaderBox(bs, payload_type);
    }

    WriteDataInformationBox(bs, payload_type);
    WriteSampleTableBox(bs, payload_type);

    NEW_SIZE(bs);
}

void DashMuxer::WriteVideoMediaHeaderBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

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
    for (size_t i = 0; i < 3; ++i)
    {
        bs.WriteBytes(2, opcolor[i]);
    }

    NEW_SIZE(bs);
}

void DashMuxer::WriteSoundMediaHeaderBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

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

void DashMuxer::WriteDataInformationBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t dinf[4] = {'d', 'i', 'n', 'f'};
    bs.WriteData(4, dinf);

    WriteDataReferenceBox(bs, payload_type);

    NEW_SIZE(bs);
}

void DashMuxer::WriteDataReferenceBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t dref[4] = {'d', 'r', 'e', 'f'};
    bs.WriteData(4, dref);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    uint32_t entry_count = 1;
    bs.WriteBytes(4, entry_count);

    for (size_t i= 0; i < entry_count; ++i)
    {
        WriteDataEntry(bs, payload_type);
    }

    NEW_SIZE(bs);
}

void DashMuxer::WriteDataEntry(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t url[4] = {'u', 'r', 'l', ' '};
    bs.WriteData(4, url);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 1;
    bs.WriteBytes(3, flags);

    NEW_SIZE(bs);
}

void DashMuxer::WriteSampleTableBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t stbl[4] = {'s', 't', 'b', 'l'};
    bs.WriteData(4, stbl);

    WriteSampleDescriptionBox(bs, payload_type);
    WriteDecodingTimeToSampleBox(bs, payload_type);
#if 1
    if (payload_type == kVideoPayload)
    {
        WriteCompositionTimeToSampleBox(bs, payload_type);
    }
#endif
    WriteSampleToChunkBox(bs, payload_type);
    WriteSampleSizeBox(bs, payload_type);
    WriteChunkOffsetBox(bs, payload_type);

    NEW_SIZE(bs);
}

void DashMuxer::WriteSampleDescriptionBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t stsd[4] = {'s', 't', 's', 'd'};
    bs.WriteData(4, stsd);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    uint32_t entry_count = 1;
    bs.WriteBytes(4, entry_count);

    if (payload_type == kVideoPayload)
    {
        WriteVisualSampleEntry(bs);
    }
    else if (payload_type == kAudioPayload)
    {
        WriteAudioSampleEntry(bs);
    }

    NEW_SIZE(bs);
}

void DashMuxer::WriteVisualSampleEntry(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t avc1[4] = {'a', 'v', 'c', '1'};
    bs.WriteData(4, avc1);

    bs.WriteBytes(6, 0);

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

void DashMuxer::WriteAudioSampleEntry(BitStream& bs)
{
    CUR_SIZE(bs);

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

void DashMuxer::WriteEsds(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t esds[4] = {'e', 's', 'd', 's'};
    bs.WriteData(4, esds);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    uint8_t mp4_es_desc_tag = 0x03;
    bs.WriteBytes(1, mp4_es_desc_tag);
    bs.WriteBytes(1, 0x16); // FIXME:cal length
    bs.WriteBytes(2, 0); // Element ID
    bs.WriteBytes(1, 0); // flags

    uint8_t mp4_dec_config_descr_tag = 0x04;
    bs.WriteBytes(1, mp4_dec_config_descr_tag);
    bs.WriteBytes(1, 0x11);
    bs.WriteBytes(1, 0x40); // object_type_id(AAC)
    bs.WriteBytes(1, 0x15); // stream_type
    bs.WriteBytes(3, 0x000300); // buffer size db
    bs.WriteBytes(4, 192000); // max bit rate
    bs.WriteBytes(4, 192000); // avg bit rate

    uint8_t mp4_desc_specific_desc_tag = 0x05;
    bs.WriteBytes(1, mp4_desc_specific_desc_tag);
    bs.WriteBytes(1, 0x02);
    bs.WriteData(2, (const uint8_t*)audio_header_.data());
    
    NEW_SIZE(bs);
}

void DashMuxer::WriteAVCC(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t avcc[4] = {'a', 'v', 'c', 'C'};
    bs.WriteData(4, avcc);

    bs.WriteData(video_header_.size(), (const uint8_t*)video_header_.data());

    //WritePixelAspectRatioBox(bs);
    
    NEW_SIZE(bs);
}

void DashMuxer::WritePixelAspectRatioBox(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t pasp[4] = {'p', 'a', 's', 'p'};
    bs.WriteData(4, pasp);

    uint32_t h_spacing = 1;
    bs.WriteBytes(4, h_spacing);

    uint32_t v_spacing = 1;
    bs.WriteBytes(4, v_spacing);
    
    NEW_SIZE(bs);
}

void DashMuxer::WriteDecodingTimeToSampleBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t stts[4] = {'s', 't', 't', 's'};
    bs.WriteData(4, stts);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

#if 1
    std::vector<Payload>& samples = (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
    uint32_t entry_count = samples.size();
    bs.WriteBytes(4, entry_count);

    uint32_t pre_sample_time = samples.empty() ? 0 : samples[0].GetDts();
    for (size_t i = 0; i < samples.size(); ++i)
    {
        uint32_t sample_count = 1;
        bs.WriteBytes(4, sample_count);

        uint32_t sample_delta = samples[i].GetDts() - pre_sample_time;
        bs.WriteBytes(4, sample_delta);
        pre_sample_time = samples[i].GetDts();
    }
#else
    std::vector<Chunk> tmp;
    for (const auto& item : chunk_)
    {
        if (item.payload_type_ == payload_type)
        {
            tmp.push_back(item);
        }
    }

    uint32_t entry_count = tmp.size();
    bs.WriteBytes(4, entry_count);

    for (const auto& item : tmp)
    {
        uint32_t sample_count = item.count_;
        bs.WriteBytes(4, sample_count);

        uint32_t sample_delta = item.delta_;
        bs.WriteBytes(4, sample_delta);
    }
#endif

    NEW_SIZE(bs);
}

void DashMuxer::WriteCompositionTimeToSampleBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

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

    for (size_t i = 0; i < samples.size(); ++i)
    {
        uint32_t sample_count = 1;
        bs.WriteBytes(4, sample_count);

        uint32_t sample_offset = samples[i].GetPts() - samples[i].GetDts();
        bs.WriteBytes(4, sample_offset);
    }

    NEW_SIZE(bs);
}

void DashMuxer::WriteSampleToChunkBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t stsc[4] = {'s', 't', 's', 'c'};
    bs.WriteData(4, stsc);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

#if 0
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
    for (const auto& item : chunk_)
    {
        if (item.payload_type_ == payload_type)
        {
            tmp.push_back(item);
        }
    }

    uint32_t entry_count = tmp.size();
    for (size_t i = 0; i < tmp.size(); ++i)
    {
        if (i > 0 && tmp[i].count_ == tmp[i-1].count_)
        {
            --entry_count;
        }
    }
    bs.WriteBytes(4, entry_count);

    for (size_t i = 0; i < tmp.size(); ++i)
    {
        if (i > 0 && tmp[i].count_ == tmp[i-1].count_)
        {
            continue;
        }

        uint32_t first_chunk = i + 1;
        bs.WriteBytes(4, first_chunk);

        uint32_t sample_per_chunk = tmp[i].count_;
        bs.WriteBytes(4, sample_per_chunk);

        uint32_t sample_description_index = 1;
        bs.WriteBytes(4, sample_description_index);
    }
#endif

    NEW_SIZE(bs);
}

void DashMuxer::WriteChunkOffsetBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t stco[4] = {'s', 't', 'c', 'o'};
    bs.WriteData(4, stco);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    std::vector<uint32_t>& chunk_offset = (payload_type == kVideoPayload) ? video_chunk_offset_ : audio_chunk_offset_;
    uint32_t entry_count = chunk_offset.size();
    bs.WriteBytes(4, entry_count);

    std::cout << LMSG << "[stco] " << "entry_count=" << entry_count << std::endl;

    for (size_t i = 0; i < chunk_offset.size(); ++i)
    {
        bs.WriteBytes(4, chunk_offset[i] + media_offset_);
    }

    NEW_SIZE(bs);
}

void DashMuxer::WriteSampleSizeBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t stsz[4] = {'s', 't', 's', 'z'};
    bs.WriteData(4, stsz);

    uint8_t version = 0;
    bs.WriteBytes(1, version);

    uint32_t flags = 0;
    bs.WriteBytes(3, flags);

    uint32_t sample_size = 0;
    bs.WriteBytes(4, sample_size);

    std::vector<Payload>& samples = (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
    uint32_t sample_count = samples.size();
    bs.WriteBytes(4, sample_count);

    for (size_t i = 0; i < samples.size(); ++i)
    {
        if (payload_type == kVideoPayload)
        {
            bs.WriteBytes(4, samples[i].GetAllLen());
        }
        else
        {
            bs.WriteBytes(4, samples[i].GetRawLen());
        }
    }

    NEW_SIZE(bs);
}
