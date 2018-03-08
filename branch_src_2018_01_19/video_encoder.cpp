#include "common_define.h"
#include "video_encoder.h"

extern "C" 
{
#include "libavutil/opt.h"
}

VideoEncoder::VideoEncoder()
    :
    av_encode_ctx_(NULL)
{
    av_init_packet(&av_packet_);
}

VideoEncoder::~VideoEncoder()
{
}

int VideoEncoder::Init(const string& encoder_name, const int& width, const int& height, const int& fps, const int& bitrate, const AVCodecContext* decode_ctx)
{
    AVCodec* codec = avcodec_find_encoder_by_name(encoder_name.c_str());

    if (codec == NULL)
    {
        cout << LMSG << "can't find video encoder [" << encoder_name << "]" << endl;
        return -1;
    }

    av_encode_ctx_ = avcodec_alloc_context3(codec);

	if (av_encode_ctx_ == NULL)
    {   
        cout << LMSG << "avcodec_alloc_context3 failed" << endl;
        return -1; 
    }

    av_encode_ctx_->time_base.num = 1;
    av_encode_ctx_->time_base.den = 1000;

    av_encode_ctx_->width = width;
    av_encode_ctx_->height = height;
    av_encode_ctx_->gop_size = fps * 3;

    av_encode_ctx_->bit_rate = bitrate;
    av_encode_ctx_->rc_min_rate = bitrate;
    av_encode_ctx_->rc_max_rate = bitrate;
    av_encode_ctx_->bit_rate_tolerance = bitrate;
	av_encode_ctx_->rc_buffer_size = bitrate * 9 / 20; //Adjust every 45% of bitrate, simulate cbr  
	av_encode_ctx_->rc_initial_buffer_occupancy = av_encode_ctx_->rc_buffer_size * 4 / 5; //0.9
	av_encode_ctx_->qcompress = 1.0; /* 0.0 => cbr, 1.0 => constant qp */

    av_encode_ctx_->color_range = decode_ctx->color_range;
    av_encode_ctx_->colorspace = decode_ctx->colorspace;
    av_encode_ctx_->color_primaries = decode_ctx->color_primaries;
    av_encode_ctx_->color_trc = decode_ctx->color_trc;
    av_encode_ctx_->sample_aspect_ratio = decode_ctx->sample_aspect_ratio;

    av_encode_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    av_encode_ctx_->thread_count = 8;

	av_opt_set(av_encode_ctx_->priv_data, "b-pyramid", "0", 0);

	int ret = avcodec_open2(av_encode_ctx_, codec, NULL);

    if (ret < 0)
    {   
        cout << LMSG << "avcodec_open2 failed" << endl;
        return ret;
    }

    cout << LMSG << "video encoder init success" << endl;

    return 0;
}

int VideoEncoder::Encode(const AVFrame* frame, int& got_packet)
{
#if 1
    cout << LMSG << "dts:" << frame->pkt_dts << ",pts:" << frame->pts << endl;

	int ret = avcodec_send_frame(av_encode_ctx_, frame);
    if (ret < 0) 
    {
        cout << LMSG << "avcodec_send_frame failed" << endl;
        return -1;
    }   

    av_init_packet(&av_packet_);
    av_packet_.data = NULL;
    av_packet_.size = 0;

    ret = avcodec_receive_packet(av_encode_ctx_, &av_packet_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        return 0;
    }
    else if (ret < 0) 
    {
        cout << LMSG << "avcodec_receive_packet failed" << endl;
        return -1;
    }   

    got_packet = 1;

#else
    int ret = avcodec_encode_video2(av_encode_ctx_, av_packet_, frame, &got_packet);

    if (ret < 0)
    {
        cout << LMSG << "avcodec_encode_video2 failed" << endl;
        return -1;
    }
#endif

    cout << LMSG << "encode video success, size:" << av_packet_.size << ",pts:" <<av_packet_.pts << ",dts:" << av_packet_.dts << endl;

    return 0;
}
