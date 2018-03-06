#include "audio_resample.h"
#include "common_define.h"

extern "C"
{
#include "libavutil/opt.h"
}

AudioResample::AudioResample()
    :
    swr_ctx_(NULL),
    resample_frame_(NULL),
	in_channel_layout_(-1),
    in_sample_rate_(-1),
    in_sample_fmt_(-1),
	out_channel_layout_(-1),
    out_sample_rate_(-1),
    out_sample_fmt_(-1)
{
}

AudioResample::~AudioResample()
{
}

int AudioResample::Init(const int& in_channel_layout, const int& out_channel_layout, 
                        const int& in_sample_rate, const int& out_sample_rate,
                        const int& in_sample_fmt, const int& out_sample_fmt)
{
    in_channel_layout_ = in_channel_layout;
    out_channel_layout_ = out_channel_layout;

    in_sample_rate_ = in_sample_rate;
    out_sample_rate_ = out_sample_rate;

    in_sample_fmt_ = in_sample_fmt;
    out_sample_fmt_ = out_sample_fmt;

    resample_frame_ = av_frame_alloc();

    swr_ctx_ = swr_alloc();

    if (swr_ctx_ == NULL)
    {
        cout << LMSG << "avresample_alloc_context failed" << endl;
        return -1;
    }

	av_opt_set_int(swr_ctx_, "in_channel_layout",  in_channel_layout, 0); 
    av_opt_set_int(swr_ctx_, "out_channel_layout", out_channel_layout, 0); 

    av_opt_set_int(swr_ctx_, "in_sample_rate", in_sample_rate, 0); 
    av_opt_set_int(swr_ctx_, "out_sample_rate", out_sample_rate, 0); 

    av_opt_set_int(swr_ctx_, "in_sample_fmt", in_sample_fmt, 0); 
    av_opt_set_int(swr_ctx_, "out_sample_fmt", out_sample_fmt, 0); 

    int ret = swr_init(swr_ctx_);

    if (ret < 0)
    {
        cout << LMSG << "swr_init failed" << endl;
        return ret;
    }

    cout << LMSG << "swr_init success" << endl;

    return 0;
}

int AudioResample::Resample(const AVFrame* frame, int& got_resample)
{
    const int out_nb_samples = 960;

    resample_frame_->nb_samples = out_nb_samples; // XXX: WHY 960
    resample_frame_->format = out_sample_fmt_;
    resample_frame_->sample_rate = out_sample_rate_;
    resample_frame_->channel_layout = out_channel_layout_;

    int ret = swr_convert_frame(swr_ctx_, resample_frame_, frame);

    if (ret < 0)
    {
        cout << LMSG << "swr_convert_frame failed" << endl;
        return ret;
    }

    resample_frame_->pts = frame->pts;
    
    cout << "resample audio success" << endl;

    got_resample = 1;

    return 0;
}
