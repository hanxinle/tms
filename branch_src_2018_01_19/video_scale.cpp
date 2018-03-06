#include "common_define.h"
#include "video_scale.h"

extern "C"
{
#include "libavutil/imgutils.h"
}

VideoScale::VideoScale()
    :
    sws_ctx_(NULL)
{
}

VideoScale::~VideoScale()
{
}

int VideoScale::Init(const AVFrame* src_frame, const int& dst_width, const int& dst_height)
{
    sws_frame_ = av_frame_alloc();
    sws_frame_->width = dst_width;
    sws_frame_->height = dst_height;
    sws_frame_->format = AV_PIX_FMT_YUV420P;

    int ret = av_image_alloc(sws_frame_->data, sws_frame_->linesize, sws_frame_->width, sws_frame_->height, (AVPixelFormat)sws_frame_->format, 32);

    if (ret < 0)
    {
        cout << LMSG << "av_image_alloc failed" << endl;
        return ret;
    }

    sws_ctx_ = sws_getContext(src_frame->width, src_frame->height, (AVPixelFormat)src_frame->format, 
                              sws_frame_->width, sws_frame_->height, (AVPixelFormat)sws_frame_->format, 
                              SWS_BICUBIC, NULL, NULL, NULL);

    if (sws_ctx_ == NULL)
    {
        cout << LMSG << "sws_getContext failed" << endl;
        return -1;
    }

    return 0;
}

int VideoScale::Scale(const AVFrame* src_frame)
{
    int ret = sws_scale(sws_ctx_, src_frame->data, src_frame->linesize, 0, src_frame->height, sws_frame_->data, sws_frame_->linesize);

    if (ret < 0)
    {
        cout << LMSG << "sws_scale failed" << endl;
        return ret;
    }

    sws_frame_->pts = src_frame->pts;

    cout << LMSG << "scale success" << endl;

    return 0;
}
