#ifndef __VIDEO_TRANS_CODER_H__
#define __VIDEO_TRANS_CODER_H__

#include "media_output.h"
#include "video_decoder.h"
#include "video_encoder.h"
#include "video_scale.h"

class VideoTransCoder
{
public:
    VideoTransCoder();
    ~VideoTransCoder();

    int InitDecoder(const string& video_header);
    int Decode(uint8_t* data, const int& size, const int64_t& dts, const int64_t& pts, int& got_picture);

    int InitScale(const AVFrame* src_frame, const int& dst_width, const int& dst_height);
    int Scale();

    int InitEncoder(const string& encoder_name, const int& width, const int& height, const int& fps, const int& bitrate);
    int Encode(const AVFrame* frame, int& got_packet);

    void SetMediaOutput(MediaOutput* media_output)
    {
        media_output_ = media_output;
    }

private:
    VideoDecoder video_decoder_;
    VideoEncoder video_encoder_;
    VideoScale   video_scale_;

    uint64_t decode_frame_count_;

    MediaOutput* media_output_;
};

#endif // __VIDEO_TRANS_CODER_H__
