#include "audio_encoder.h"
#include "common_define.h"

AudioEncoder::AudioEncoder()
{
}

AudioEncoder::~AudioEncoder()
{
}

int AudioEncoder::Init()
{
    string encoder_name = "libopus";

    AVCodec* codec = avcodec_find_encoder_by_name(encoder_name.c_str());

    if (codec == NULL)
    {
        cout << LMSG << "can't find audio encoder [" << encoder_name << "]" << endl;
        return -1;
    }

    av_encode_ctx_ = avcodec_alloc_context3(codec);

    if (av_encode_ctx_ == NULL)
    {
        cout << LMSG << "avcodec_alloc_context3 failed" << endl;
        return -1;
    }

    av_encode_ctx_->sample_rate = 48000;
    av_encode_ctx_->channels = 2;
    av_encode_ctx_->sample_fmt = AV_SAMPLE_FMT_S16;
    av_encode_ctx_->frame_size = 960;
    av_encode_ctx_->channel_layout = AV_CH_LAYOUT_STEREO;

    int ret = avcodec_open2(av_encode_ctx_, codec, NULL);

    if (ret < 0)
    {
        cout << LMSG << "avcodec_open2 failed" << endl;
        return ret;
    }

    cout << LMSG << "audio encoder init success" << endl;

    return 0;
}

int AudioEncoder::Encode(const AVFrame* frame, int& got_packet)
{
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

    cout << LMSG << "encode audio success, size:" << av_packet_.size << ",pts:" <<av_packet_.pts << ",dts:" << av_packet_.dts << endl;

    return 0;
}
