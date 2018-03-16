#ifndef __AUDIO_DECODER_H__
#define __AUDIO_DECODER_H__

#include <string>

extern "C"
{
#include "libavcodec/avcodec.h"
}

using std::string;

class AudioDecoder
{
public:
    AudioDecoder();
    ~AudioDecoder();

    int Init(const string& audio_header);
    int Decode(uint8_t* data, const int& size, const int64_t& dts, int& got_audio);

    AVFrame* GetDecodedFrame()
    {
        return av_frame_;
    }

private:
    AVCodecContext* av_decode_ctx_;
    AVFrame* av_frame_;
    AVPacket av_packet_;
};

#endif // __AUDIO_DECODER_H__
