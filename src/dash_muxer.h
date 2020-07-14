#ifndef __DASH_MUXER_H__
#define __DASH_MUXER_H__

#include <string>
#include <vector>

#include "ref_ptr.h"

class BitStream;


class DashMuxer 
{
public:
    DashMuxer();
    ~DashMuxer();

    struct Chunk
    {
        int count_;
        int size_;
        PayloadType payload_type_;
        int delta_;
        uint64_t dts_;

        Chunk();
        Chunk(const Chunk& rhs);
        Chunk& operator=(const Chunk& rhs);
    };

	void OpenDumpFile(const std::string& file);
	void Dump(const uint8_t* data, const int& len);

	int OnAudio(const Payload& payload);
    int OnVideo(const Payload& payload);
    void CalChunk(const uint64_t dts, const uint64_t& len, const PayloadType& payload_type);
    int OnMetaData(const std::string& metadata);
    int OnVideoHeader(const std::string& video_header);
    int OnAudioHeader(const std::string& audio_header);
private:
    void Flush();
    void Reset();
    void WriteFileTypeBox(BitStream& bs);
    void WriteFreeBox(BitStream& bs);
    void WriteMovieBox(BitStream& bs);
    void WriteMediaDataBox(BitStream& bs);
    void WriteMovieHeaderBox(BitStream& bs);
    void WriteTrackBox(BitStream& bs, const PayloadType& payload_type);
    void WriteTrackHeaderBox(BitStream& bs, const PayloadType& payload_type);
    void WriteEditBox(BitStream& bs, const PayloadType& payload_type);
    void WriteEditListBox(BitStream& bs, const PayloadType& payload_type);
    void WriteMediaBox(BitStream& bs, const PayloadType& payload_type);
    void WriteMediaHeaderBox(BitStream& bs, const PayloadType& payload_type);
    void WriteHandlerReferenceBox(BitStream& bs, const PayloadType& payload_type);
    void WriteMediaInfoBox(BitStream& bs, const PayloadType& payload_type);
    void WriteVideoMediaHeaderBox(BitStream& bs, const PayloadType& payload_type);
    void WriteSoundMediaHeaderBox(BitStream& bs, const PayloadType& payload_type);
    void WriteDataInformationBox(BitStream& bs, const PayloadType& payload_type);
    void WriteDataReferenceBox(BitStream& bs, const PayloadType& payload_type);
    void WriteDataEntry(BitStream& bs, const PayloadType& payload_type);
    void WriteSampleTableBox(BitStream& bs, const PayloadType& payload_type);
    void WriteSampleDescriptionBox(BitStream& bs, const PayloadType& payload_type);
    void WriteVisualSampleEntry(BitStream& bs);
    void WriteAudioSampleEntry(BitStream& bs);
    void WriteEsds(BitStream& bs);
    void WriteAVCC(BitStream& bs);
    void WritePixelAspectRatioBox(BitStream& bs);
    void WriteDecodingTimeToSampleBox(BitStream& bs, const PayloadType& payload_type);
    void WriteCompositionTimeToSampleBox(BitStream& bs, const PayloadType& payload_type);
    void WriteSampleToChunkBox(BitStream& bs, const PayloadType& payload_type);
    void WriteChunkOffsetBox(BitStream& bs, const PayloadType& payload_type);
    void WriteSampleSizeBox(BitStream& bs, const PayloadType& payload_type);
private:
    std::string video_header_;
    std::string audio_header_;
    std::string mdat_;
    uint32_t chunk_offset_;
    uint32_t media_offset_;
    std::vector<Chunk> chunk_;
    std::vector<uint32_t> video_chunk_offset_;
    std::vector<uint32_t> audio_chunk_offset_;
    std::vector<Payload> video_samples_;
    std::vector<Payload> audio_samples_;
private:
    int dump_fd_;
};

#endif // __DASH_MUXER_H__
