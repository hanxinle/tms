#ifndef __TS_READER_H__
#define __TS_READER_H__

#include <functional>

class BitBuffer;
class Payload;

class TsReader
{
public:
    TsReader();
    ~TsReader();

    int ParseTs(const uint8_t* data, const int& len);
    int ParseTsSegment(const uint8_t* data, const int& len);

    void SetFrameCallback(std::function<void(const Payload&)> cb) { frame_callback_ = cb; }
    void SetHeaderCallback(std::function<void(const Payload&)> cb) { header_callback_ = cb; }

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

    void OpenVideoDumpFile();
    void DumpVideo(const uint8_t* data, const size_t& len);
    void OpenAudioDumpFile();
    void DumpAudio(const uint8_t* data, const size_t& len);

    void OnVideo(BitBuffer& bit_buffer, const uint32_t& pts, const uint32_t& dts);
    void OnAudio(BitBuffer& bit_buffer, const uint32_t& pts);

private:
    uint16_t pmt_pid_;
    uint16_t video_pid_;
    uint16_t audio_pid_;

    std::string video_pes_;
    std::string audio_pes_;

    int video_dump_fd_;
    int audio_dump_fd_;

    std::function<void(const Payload&)> frame_callback_;
    std::function<void(const Payload&)> header_callback_;
};

#endif // __TS_READER_H__
