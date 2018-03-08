#ifndef __VIDEO_DECODER_H__
#define __VIDEO_DECODER_H__

#include <string>

extern "C"
{
#include "libavcodec/avcodec.h"
}

using std::string;

class VideoDecoder
{
public:
    VideoDecoder();
    ~VideoDecoder();

    int Init(const string& video_header);
    int Decode(uint8_t* data, const int& size, const int64_t& dts, const int64_t& pts, int& got_picture);

    AVFrame* GetDecodedFrame()
    {
        return av_frame_;
    }

    AVCodecContext* GetCodecContext()
    {
        return av_decode_ctx_;
    }

private:
    AVCodecContext* av_decode_ctx_;
    AVPacket av_packet_;
    AVFrame* av_frame_;
};

#endif // __VIDEO_DECODER_H__
