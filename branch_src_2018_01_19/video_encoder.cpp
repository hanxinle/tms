#include "common_define.h"
#include "video_encoder.h"

VideoEncoder::VideoEncoder()
    :
    av_encode_ctx_(NULL)
{
    av_init_packet(&av_packet_);
}

VideoEncoder::~VideoEncoder()
{
}

int VideoEncoder::Init(const string& encoder_name, const int& width, const int& height, const int& fps, const int& bitrate)
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
    av_encode_ctx_->bit_rate = bitrate;
    av_encode_ctx_->gop_size = fps * 3;
    av_encode_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    av_encode_ctx_->thread_count = 8;

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
