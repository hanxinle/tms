#include "common_define.h"
#include "media_output.h"

using namespace std;

MediaOutput::MediaOutput()
    :
    avformat_ctx_(NULL),
    avstream_audio_(NULL),
    avstream_video_(NULL)
{
}

MediaOutput::~MediaOutput()
{
}

int MediaOutput::Init(const string& file_name)
{
    file_name_ = file_name;

    avformat_ctx_ = avformat_alloc_context();

    if (avformat_ctx_ == NULL)
    {
        cout << LMSG << "avformat_alloc_context failed" << endl;
        return -1;
    }

    AVOutputFormat* format = av_guess_format(NULL, file_name.c_str(), NULL);
    if (format == NULL)
    {
        cout << LMSG << "av_guess_format failed" << endl;
        return -1;
    }

    avformat_ctx_->oformat = format;

    cout << LMSG << "success" << endl;

    return 0;
}

int MediaOutput::OpenFile()
{
	int ret = avio_open(&avformat_ctx_->pb, file_name_.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0)
    {   
        cout << LMSG << "avio_open failed" << endl; 
        return ret;
    }   

    ret = avformat_write_header(avformat_ctx_, NULL);
    if (ret < 0)
    {   
        cout << LMSG << "avformat_write_header failed" << endl;
        return ret;
    }   

    cout << LMSG << "success" << endl;

    if (! video_cache_.empty())
    {
        for (const auto& video : video_cache_)
        {
            AVPacket packet;
            av_init_packet(&packet);

            packet.stream_index = avstream_video_->index;
            packet.dts = video.dts_;
            packet.data = (uint8_t*)video.data_.data();
            packet.size = video.data_.size();

            int ret = av_write_frame(avformat_ctx_, &packet);

            if (ret < 0)
            {
                cout << LMSG << "av_write_frame failed" << endl;
            }
            else
            {
                cout << LMSG << "success" << endl;
            }
        }

        video_cache_.clear();
    }

    if (! audio_cache_.empty())
    {
        for (const auto& audio : audio_cache_)
        {
            AVPacket packet;
            av_init_packet(&packet);

            packet.stream_index = avstream_audio_->index;
            packet.dts = audio.dts_;
            packet.data = (uint8_t*)audio.data_.data();
            packet.size = audio.data_.size();

            int ret = av_write_frame(avformat_ctx_, &packet);

            if (ret < 0)
            {
                cout << LMSG << "av_write_frame failed" << endl;
            }
            else
            {
                cout << LMSG << "success" << endl;
            }
        }

        audio_cache_.clear();
    }

    return 0;
}

int MediaOutput::InitAudioStream(const AVCodecContext* audio_encode_ctx)
{
    avstream_audio_ = avformat_new_stream(avformat_ctx_, NULL);

    if (avstream_audio_ == NULL)
    {
        cout << LMSG << "avformat_new_stream failed" << endl;
        return -1;
    }

    avstream_audio_->time_base = (AVRational){1, audio_encode_ctx->sample_rate};

    int ret = avcodec_parameters_from_context(avstream_audio_->codecpar, audio_encode_ctx);
    if (ret < 0)
    {
        cout << LMSG << "avcodec_parameters_from_context failed" << endl;
    }
    else
    {
        cout << LMSG << "success" << endl;
    }

    if (avstream_video_ != NULL)
    {
        OpenFile();
    }


    return ret;
}

int MediaOutput::InitVideoStream(const AVCodecContext* video_encode_ctx)
{
    avstream_video_ = avformat_new_stream(avformat_ctx_, NULL);

    if (avstream_video_ == NULL)
    {
        cout << LMSG << "avformat_new_stream failed" << endl;
        return -1;
    }

    avstream_video_->time_base = video_encode_ctx->time_base;

    int ret = avcodec_parameters_from_context(avstream_video_->codecpar, video_encode_ctx);
    if (ret < 0)
    {
        cout << LMSG << "avcodec_parameters_from_context failed" << endl;
    }
    else
    {
        cout << LMSG << "success" << endl;
    }

    if (avstream_audio_ != NULL)
    {
        OpenFile();
    }

    return ret;
}

int MediaOutput::WriteVideo(AVPacket* packet)
{
    if (avstream_audio_ == NULL)
    {
        video_cache_.emplace_back((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                  (int64_t)packet->dts, (int)packet->flags, MediaVideo);

        return 0;
    }

    packet->stream_index = avstream_video_->index;

    int ret = av_write_frame(avformat_ctx_, packet);

    if (ret < 0)
    {
        cout << LMSG << "av_write_frame failed" << endl;
    }
    else
    {
        cout << LMSG << "success" << endl;
    }

    return ret;
}

int MediaOutput::WriteAudio(AVPacket* packet)
{
    if (avstream_video_ == NULL)
    {
        audio_cache_.emplace_back((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                  (int64_t)packet->dts, (int)packet->flags, MediaAudio);

        return 0;
    }

    packet->stream_index = avstream_audio_->index;

    int ret = av_write_frame(avformat_ctx_, packet);

    if (ret < 0)
    {
        cout << LMSG << "av_write_frame failed" << endl;
    }
    else
    {
        cout << LMSG << "success" << endl;
    }

    return ret;
}
