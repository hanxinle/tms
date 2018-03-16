#include <fcntl.h>
#include <unistd.h>

#include "audio_resample.h"
#include "common_define.h"

extern "C"
{
#include "libavutil/opt.h"
}

AudioResample::AudioResample()
    :
    resample_ctx_(NULL),
    resample_frame_(NULL),
	in_channel_layout_(-1),
    in_sample_rate_(-1),
    in_sample_fmt_(-1),
	out_channel_layout_(-1),
    out_sample_rate_(-1),
    out_sample_fmt_(-1),
    resample_pcm_fd_(-1),
    samples_count_(0),
    fake_dts_(true)
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

    resample_ctx_ = avresample_alloc_context();

    if (resample_ctx_ == NULL)
    {
        cout << LMSG << "avresample_alloc_context failed" << endl;
        return -1;
    }

	av_opt_set_int(resample_ctx_, "in_channel_layout",  in_channel_layout, 0); 
    av_opt_set_int(resample_ctx_, "out_channel_layout", out_channel_layout, 0); 

    av_opt_set_int(resample_ctx_, "in_sample_rate", in_sample_rate, 0); 
    av_opt_set_int(resample_ctx_, "out_sample_rate", out_sample_rate, 0); 

    av_opt_set_int(resample_ctx_, "in_sample_fmt", in_sample_fmt, 0); 
    av_opt_set_int(resample_ctx_, "out_sample_fmt", out_sample_fmt, 0); 

    int ret = avresample_open(resample_ctx_);

    if (ret < 0)
    {
        cout << LMSG << "swr_init failed" << endl;
        return ret;
    }

    cout << LMSG << "swr_init success" << endl;

    return 0;
}

int AudioResample::Resample(const AVFrame* frame)
{
    cout << LMSG << "src pts:" << frame->pts << endl;

    int ret = avresample_convert(resample_ctx_, NULL, 0, 0, frame->data, frame->linesize[0], frame->nb_samples);

    if (ret < 0)
    {
        cout << LMSG << "swr_convert_frame failed" << endl;
    }

    return ret;
}

int AudioResample::GetFrameFromFifo(int& got_resample)
{
    //const int out_nb_samples = av_rescale_rnd(frame->nb_samples, out_sample_rate_, in_sample_rate_, AV_ROUND_UP);
    const int out_nb_samples = 960;

	resample_frame_->nb_samples = out_nb_samples;
    resample_frame_->format = out_sample_fmt_;
    resample_frame_->sample_rate = out_sample_rate_;
    resample_frame_->channel_layout = out_channel_layout_;

    cout << LMSG << "avresample_available(resample_ctx_) = " << avresample_available(resample_ctx_) << endl;
    if (avresample_available(resample_ctx_) > resample_frame_->nb_samples)
    {   
        if (av_frame_get_buffer(resample_frame_, 1) < 0)
        {   
            cout << LMSG << "av_frame_get_buffer" << endl;
            return -1; 
        }   

        if (avresample_read(resample_ctx_, resample_frame_->extended_data, resample_frame_->nb_samples) == resample_frame_->nb_samples)
        {   
            if (resample_pcm_fd_ > 0)
            {
                ssize_t bytes = write(resample_pcm_fd_, resample_frame_->extended_data, resample_frame_->linesize[0]);
                cout << LMSG << "write resample pcm " << bytes << " bytes" << endl;
            }

            if (fake_dts_)
            {
                resample_frame_->pts = (double)samples_count_ / (double)out_sample_rate_ * 1000;
                resample_frame_->pkt_dts = resample_frame_->pts;
                resample_frame_->pkt_pts = resample_frame_->pts;
            }
            else
            {
                resample_frame_->pts = av_rescale_q(samples_count_, (AVRational){1, in_sample_rate_}, (AVRational){1, 1000});
                //resample_frame_->pkt_dts = frame->pkt_dts;
                //resample_frame_->pkt_pts = frame->pkt_pts;
            }
            
            cout << "resample audio success, pts:" << resample_frame_->pts << ",dts:" << resample_frame_->pkt_dts << endl;

            got_resample = 1;

            samples_count_ += out_nb_samples;

            return 0;
        }   
    }

    return -1;
}

int AudioResample::OpenPcmFd()
{
    if (resample_pcm_fd_ > 0)
    {
        return 0;
    }

    resample_pcm_fd_ = open("resample.pcm", O_CREAT|O_TRUNC|O_RDWR, 0664);
    
    if (resample_pcm_fd_ < 0)
    {
        cout << LMSG << "open resample.pcm failed" << endl;
        return -1;
    }

    return 0;
}
