#include "audio_decoder.h"
#include "common_define.h"


AudioDecoder::AudioDecoder()
    :
    av_decode_ctx_(NULL)
{
	av_init_packet(&av_packet_);
    av_packet_.data = NULL;
    av_packet_.size = 0;

    av_frame_ = av_frame_alloc();
}

AudioDecoder::~AudioDecoder()
{
}

int AudioDecoder::Init(const string& audio_header)
{
    string decoder_name = "libfdk_aac";

    AVCodec* codec = avcodec_find_decoder_by_name(decoder_name.c_str());

    if (codec == NULL)
    {
        cout << LMSG << "can't find decoder [" << decoder_name << "]" << endl;
        return -1;
    }

    av_decode_ctx_ = avcodec_alloc_context3(codec);

    if (av_decode_ctx_ == NULL)
    {
        cout << LMSG << "avcodec_alloc_context3 failed" << endl;
        return -1;
    }

	av_decode_ctx_->extradata_size = 4096;
    av_decode_ctx_->extradata = (uint8_t*)av_malloc(av_decode_ctx_->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
    memset(av_decode_ctx_->extradata, 0, av_decode_ctx_->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);

    memcpy(av_decode_ctx_->extradata, (uint8_t*)audio_header.data(), audio_header.size());
    av_decode_ctx_->extradata_size = audio_header.size();

    int ret = avcodec_open2(av_decode_ctx_, codec, NULL);

    if (ret < 0)
    {
        cout << LMSG << "avcodec_open2 failed" << endl;
        return ret;
    }

    return 0;
}

int AudioDecoder::Decode(uint8_t* data, const int& size, const int64_t& dts, int& got_audio)
{
	av_packet_.data = data;
    av_packet_.size = size;
    av_packet_.dts = dts;
    av_packet_.pts = dts;

    int peek_len = 48; 
    if (peek_len > size)
    {   
        peek_len = size;
    }   

    cout << LMSG << "peek \n" << Util::Bin2Hex(data, peek_len) << endl;

    int ret = avcodec_decode_audio4(av_decode_ctx_, av_frame_, &got_audio, &av_packet_);

    if (got_audio)
    {   
        cout << LMSG << "decode audio success" << endl;
    }

    return ret;
}
