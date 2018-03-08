#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

extern "C"
{
#include "libavcodec/avcodec.h"
}

class VideoEncoder
{
public:
    VideoEncoder();
    ~VideoEncoder();

    int Init(const string& encoder_name, const int& width, const int& height, const int& fps, const int& bitrate, const AVCodecContext* decode_ctx);
    int Encode(const AVFrame* frame, int& got_packet);

    AVPacket* GetEncodePacket()
    {
        return &av_packet_;
    }

private:
    AVCodecContext* av_encode_ctx_;
    AVPacket av_packet_;
};

#endif // __VIDEO_ENCODER_H__
