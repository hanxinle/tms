#include "common_define.h"
#include "video_decoder.h"

static string GetAVError(const int& err)
{
    char err_desc[1024];
    av_strerror(err, (char*)(&err_desc), sizeof(err_desc));

    return string(err_desc);
}

VideoDecoder::VideoDecoder()
    :
    av_decode_ctx_(NULL)
{
    av_init_packet(&av_packet_);
    av_packet_.data = NULL;
    av_packet_.size = 0;

    av_frame_ = av_frame_alloc();
}

VideoDecoder::~VideoDecoder()
{
}

int VideoDecoder::Init(const string& video_header)
{
    string decoder_name = "h264";
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

    memcpy(av_decode_ctx_->extradata, (uint8_t*)video_header.data(), video_header.size());
    av_decode_ctx_->extradata_size = video_header.size();

    int ret = avcodec_open2(av_decode_ctx_, codec, NULL);

    if (ret < 0)
    {
        cout << LMSG << "avcodec_open2 failed" << endl;
        return ret;
    }

    return 0;
}

int VideoDecoder::Decode(uint8_t* data, const int& size, const int64_t& dts, const int64_t& pts, int& got_picture)
{
    av_packet_.data = data;
    av_packet_.size = size;
    av_packet_.dts = dts;
    av_packet_.pts = pts;

    int peek_len = 48;
    if (peek_len > size)
    {
        peek_len = size;
    }


    cout << LMSG << "peek \n" << Util::Bin2Hex(data, peek_len) << endl;

    int ret = avcodec_decode_video2(av_decode_ctx_, av_frame_, &got_picture, &av_packet_);

    if (got_picture)
    {
        cout << LMSG << "decode success, width:" << av_frame_->width << ", height:" << av_frame_->height << ", dts:" << av_frame_->pkt_dts << ", pts:" << av_frame_->pts << endl;
    }

    return ret;
}
