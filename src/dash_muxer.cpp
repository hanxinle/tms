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

    return kSuccess;
}

int DashMuxer::OnVideo(const Payload& payload)
{
    if (payload.IsIFrame())
    {
        Flush();
    }

    video_samples_.push_back(payload);

    return kSuccess;
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
    return kSuccess;
}

void DashMuxer::Flush()
{
    if (video_samples_.empty() && audio_samples_.empty())
    {
        return;
    }

    size_t buf_size = 10 * 1024 * 1024;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    BitStream bs(buf, buf_size);
    WriteFileTypeBox(bs);
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

    chunk_offset_ = bs.SizeInBytes();

    auto iter_v = video_samples_.begin();
    auto iter_a = audio_samples_.begin();

    std::cout << "v:" << video_samples_.size() << ",a:" << audio_samples_.size() << std::endl;
    while (iter_v != video_samples_.end() || iter_a != audio_samples_.end())
    {
        if (iter_v == video_samples_.end())
        {
            bs.WriteData(iter_a->GetRawLen(), iter_a->GetRawData());
            audio_chunk_offset_.push_back(chunk_offset_);
            chunk_offset_ += iter_a->GetRawLen();
            std::cout << "1" << std::endl;
            ++iter_a;
        }
        else if (iter_a == audio_samples_.end())
        {
            bs.WriteData(iter_v->GetRawLen(), iter_v->GetRawData());
            video_chunk_offset_.push_back(chunk_offset_);
            chunk_offset_ += iter_v->GetRawLen();
            std::cout << "2" << std::endl;
            ++iter_v;
        }
        else
        {
            std::cout << "3" << std::endl;
            if (iter_a->GetPts() < iter_v->GetPts())
            {
                bs.WriteData(iter_a->GetRawLen(), iter_a->GetRawData());
                audio_chunk_offset_.push_back(chunk_offset_);
                chunk_offset_ += iter_a->GetRawLen();
                ++iter_a;
            }
            else
            {
                bs.WriteData(iter_v->GetRawLen(), iter_v->GetRawData());
                video_chunk_offset_.push_back(chunk_offset_);
                chunk_offset_ += iter_v->GetRawLen();
                ++iter_v;
            }
        }
    }

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

        uint32_t duration = 1000;
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

    uint32_t matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x4000000 };
    for (size_t i = 0; i < 9; ++i)
    {
        bs.WriteBytes(4, matrix[i]);
    }

    uint32_t pre_defined[6] = {0, 0, 0, 0, 0, 0};
    for (size_t i = 0; i < 6; ++i)
    {
        bs.WriteBytes(4, pre_defined[i]);
    }

    uint32_t next_trace_id = 0;
    bs.WriteBytes(4, next_trace_id);

    NEW_SIZE(bs);
}

void DashMuxer::WriteTrackBox(BitStream& bs, const PayloadType& payload_type)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t trak[4] = {'t', 'r', 'a', 'k'};
    bs.WriteData(4, trak);

    WriteTrackHeaderBox(bs, payload_type);
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

        uint32_t track_ID = 1;
        bs.WriteBytes(4, track_ID);

        uint32_t reversed = 0;
        bs.WriteBytes(4, reversed);

        uint32_t duration = 1000;
        bs.WriteBytes(4, duration);
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

    uint32_t matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x4000000 };
    for (size_t i = 0; i < 9; ++i)
    {
        bs.WriteBytes(4, matrix[i]);
    }

    uint32_t width = 1920;
    bs.WriteBytes(4, width);

    uint32_t height = 1080;
    bs.WriteBytes(4, height);

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

        uint32_t duration = 1000;
        bs.WriteBytes(4, duration);
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

    static uint8_t vide[4] = {'v', 'i', 'd', 'e'};
    static uint8_t soun[4] = {'s', 'o', 'u', 'n'};

    if (payload_type == kVideoPayload)
    {
        bs.WriteData(4, vide);
    }
    else if (payload_type == kAudioPayload)
    {
        bs.WriteData(4, soun);
    }

    bs.WriteBytes(4, 0);
    bs.WriteBytes(4, 0);
    bs.WriteBytes(4, 0);

    std::string debug = "VideoHandler";
    if (payload_type == kAudioPayload)
    {
        debug = "AudioHandler";
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

    uint32_t flags = 1;
    bs.WriteBytes(3, flags);

    uint16_t balance = 0;
    bs.WriteBytes(2, balance);

    uint16_t reserved = 0;
    bs.WriteBytes(2, reserved);

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
    WriteSampleToChunkBox(bs, payload_type);
    WriteChunkOffsetBox(bs, payload_type);
    WriteSampleSizeBox(bs, payload_type);

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

    std::string compressorname(32, 'c');
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

    uint32_t samplerate = 44100;
    bs.WriteBytes(4, samplerate);

    NEW_SIZE(bs);
}

void DashMuxer::WriteAVCC(BitStream& bs)
{
    CUR_SIZE(bs);

    bs.WriteBytes(4, 0);
    static uint8_t avcc[4] = {'a', 'v', 'c', 'C'};
    bs.WriteData(4, avcc);

    bs.WriteData(video_header_.size(), (const uint8_t*)video_header_.data());
    
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

    std::vector<Payload>& samples = (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
    uint32_t entry_count = samples.size();
    bs.WriteBytes(4, entry_count);

    uint32_t pre_sample_time = 0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        uint32_t sample_count = 1;
        bs.WriteBytes(4, sample_count);

        uint32_t sample_delta = samples[i].GetDts() - pre_sample_time;
        bs.WriteBytes(4, sample_delta);
        pre_sample_time = samples[i].GetDts();
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
    std::vector<Payload>& samples = (payload_type == kVideoPayload) ? video_samples_ : audio_samples_;
    uint32_t entry_count = samples.size();
    bs.WriteBytes(4, entry_count);

    for (size_t i = 0; i < samples.size(); ++i)
    {
        uint32_t first_chunk = i + 1;
        bs.WriteBytes(4, first_chunk);

        uint32_t sample_per_chunk = 1;
        bs.WriteBytes(4, sample_per_chunk);

        uint32_t sample_description_index = 1;
        bs.WriteBytes(4, sample_description_index);
    }
#else
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

    for (size_t i = 0; i < chunk_offset.size(); ++i)
    {
        bs.WriteBytes(4, chunk_offset[i]);
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
        bs.WriteBytes(4, samples[i].GetRawLen());
    }

    NEW_SIZE(bs);
}
