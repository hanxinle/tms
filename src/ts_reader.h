#ifndef __TS_READER_H__
#define __TS_READER_H__

class BitBuffer;

class TsReader
{
public:
    TsReader();
    ~TsReader();

    int ParseTs(const uint8_t* data, const int& len);
    int ParseTsSegment(const uint8_t* data, const int& len);

private:
    int ParsePAT(BitBuffer& bit_buffer);
    int ParsePMT(BitBuffer& bit_buffer);
    int ParseAdaptation(BitBuffer& bit_buffer);
    int ParsePES(std::string& pes);

    int CollectAudioPES(BitBuffer& bit_buffer);
    int CollectVideoPES(BitBuffer& bit_buffer);

    int PackHead(BitBuffer& bit_buffer);
    int SystemHeader(BitBuffer& bit_buffer);

private:
    uint16_t pmt_pid_;
    uint16_t video_pid_;
    uint16_t audio_pid_;

    std::string video_pes_;
    std::string audio_pes_;

    int video_dump_fd_;
};

#endif // __TS_READER_H__
