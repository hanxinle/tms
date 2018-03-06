#ifndef __VIDEO_SCALE_H__
#define __VIDEO_SCALE_H__

extern "C"
{
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
}

class VideoScale
{
public:
    VideoScale();
    ~VideoScale();

    int Init(const AVFrame* src_frame, const int& dst_width, const int& dst_height);
    int Scale(const AVFrame* src_frame);

    AVFrame* GetScaleFrame()
    {
        return sws_frame_;
    }

private:
    SwsContext* sws_ctx_;
    AVFrame* sws_frame_;
};

#endif // __VIDEO_SCALE_H__
