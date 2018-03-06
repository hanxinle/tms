#ifndef __AUDIO_ENCODER_H__
#define __AUDIO_ENCODER_H__

extern "C"
{
#include "libavcodec/avcodec.h"
}

class AudioEncoder
{
public:
    AudioEncoder();
    ~AudioEncoder();

    int Init();
    int Encode(const AVFrame* frame, int& got_packet);

    AVPacket* GetEncodePacket()
    {   
        return &av_packet_;
    }

private:
    AVPacket av_packet_;
    AVCodecContext* av_encode_ctx_;
};

#endif // __AUDIO_ENCODER_H__
