#include "common_define.h"
#include "media_output.h"

using namespace std;

MediaOutput::MediaOutput()
    :
    avformat_ctx_(NULL),
    avstream_audio_(NULL),
    avstream_video_(NULL),
    use_buffer_(true),
    fake_dts_(true),
    audio_dts_(0),
    video_dts_(0),
    video_fps_(30),
    audio_fps_(50)
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
            InternalWriteMedia(video);
        }

        video_cache_.clear();
    }

    if (! audio_cache_.empty())
    {
        for (const auto& audio : audio_cache_)
        {
            InternalWriteMedia(audio);
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

    if (use_buffer_)
    {
        if (media_buffer_.empty())
        {
            media_buffer_.emplace_back((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                      (int64_t)packet->dts, (int)packet->flags, MediaVideo);
        }
        else
        {
            MediaPacket media_packet((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                     (int64_t)packet->dts, (int)packet->flags, MediaVideo);

            bool inserted = false;
    
            for (auto iter = media_buffer_.rbegin(); iter != media_buffer_.rend(); ++iter)
            {
                if (iter->dts_ < packet->dts)
                {
                    media_buffer_.insert(iter.base(), media_packet);
                    inserted = true;
                    break;
                }
            }

            if (! inserted)
            {
                cout << LMSG << "insert to begin" << endl;
                media_buffer_.insert(media_buffer_.begin(), media_packet);
            }
        }
    
        if (media_buffer_.size() >= 50)
        {
            auto iter = media_buffer_.begin();
    
            for (auto iter = media_buffer_.begin(); iter != media_buffer_.end(); ++iter)
            {
                cout << "IsAudio:" << iter->IsAudio() << ",dts:" << iter->dts_ << endl;
            }
    
            while (media_buffer_.size() >= 10 && media_buffer_.rbegin()->dts_ - media_buffer_.begin()->dts_ >= 5000)
            {
                cout << LMSG << "diff:" << (media_buffer_.rbegin()->dts_ - media_buffer_.begin()->dts_) << endl;

                InternalWriteMedia(*iter);
    
                iter = media_buffer_.erase(iter);
            }
        }

        return 0;
    }
    else
    {
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

    return -1;
}

int MediaOutput::WriteAudio(AVPacket* packet)
{
    if (avstream_video_ == NULL)
    {
        audio_cache_.emplace_back((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                  (int64_t)packet->dts, (int)packet->flags, MediaAudio);

        return 0;
    }

    if (use_buffer_)
    {
        if (media_buffer_.empty())
        {
            media_buffer_.emplace_back((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                      (int64_t)packet->dts, (int)packet->flags, MediaAudio);
        }
        else
        {
            MediaPacket media_packet((const uint8_t*)packet->data, (int)packet->size, (int64_t)packet->dts,
                                     (int64_t)packet->dts, (int)packet->flags, MediaAudio);

            bool inserted = false;
            for (auto iter = media_buffer_.rbegin(); iter != media_buffer_.rend(); ++iter)
            {
                if (iter->dts_ < packet->dts)
                {
                    media_buffer_.insert(iter.base(), media_packet);
                    inserted = true;
                    break;
                }
            }

            if (! inserted)
            {
                cout << LMSG << "insert to begin" << endl;
                media_buffer_.insert(media_buffer_.begin(), media_packet);
            }
        }


        if (media_buffer_.size() >= 50)
        {
            auto iter = media_buffer_.begin();

            for (auto iter = media_buffer_.begin(); iter != media_buffer_.end(); ++iter)
            {
                cout << "IsAudio:" << iter->IsAudio() << ",dts:" << iter->dts_ << endl;
            }

            while (media_buffer_.size() >= 10 && media_buffer_.rbegin()->dts_ - media_buffer_.begin()->dts_ >= 5000)
            {
                cout << LMSG << "diff:" << (media_buffer_.rbegin()->dts_ - media_buffer_.begin()->dts_) << endl;
                InternalWriteMedia(*iter);

                iter = media_buffer_.erase(iter);
            }
        }

        return 0;
    }
    else
    {
        InternalWriteMedia(packet, true);
    }

    return -1;
}

int MediaOutput::InternalWriteMedia(AVPacket* av_packet, const bool& is_audio)
{
    if (is_audio)
    {
        return 0;
        av_packet->stream_index = avstream_audio_->index;
    }
    else
    {
        av_packet->stream_index = avstream_video_->index;
    }

    int ret = av_write_frame(avformat_ctx_, av_packet);
    
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

int MediaOutput::InternalWriteMedia(MediaPacket media_packet)
{
    AVPacket packet;
    av_init_packet(&packet);

    if (media_packet.IsAudio())
    {
        packet.stream_index = avstream_audio_->index;
    }
    else
    {
        packet.stream_index = avstream_video_->index;
    }

    if (! fake_dts_)
    {
        packet.dts = media_packet.dts_;
    }
    else
    {
        if (media_packet.IsAudio())
        {
            packet.dts = audio_dts_;
            audio_dts_ += 1000 / audio_fps_;
        }
        else if (media_packet.IsVideo())
        {
            packet.dts = video_dts_;
            video_dts_ += 1000 / video_fps_;
        }
    }

    packet.pts = media_packet.dts_;
    packet.data = (uint8_t*)media_packet.data_.data();
    packet.size = media_packet.data_.size();

    return InternalWriteMedia(&packet, media_packet.IsAudio());
}
