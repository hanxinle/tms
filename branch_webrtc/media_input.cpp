#include <iostream>

#include "common_define.h"
#include "media_input.h"

using namespace std;

MediaInput::MediaInput()
    :
    avformat_ctx_(NULL),
    audio_stream_index_(-1),
    video_stream_index_(-1)
{
    av_init_packet(&packet_);
    packet_.data = NULL;
    packet_.size = 0;
}

MediaInput::~MediaInput()
{
}

static string GetAVError(const int& err)
{
    char err_desc[1024];
    av_strerror(err, (char*)(&err_desc), sizeof(err_desc));

    return string(err_desc);
}

int MediaInput::Open(const string& file)
{
    file_name_ = file;

	int ret = avformat_open_input(&avformat_ctx_, file.c_str(), NULL, NULL);
    if (ret != 0)
    {
        cout << LMSG << "avformat_open_input ret:" << ret << ", err:" << GetAVError(ret) << endl;
        return ret;
    }

    ret = avformat_find_stream_info(avformat_ctx_, NULL);
    if (ret != 0)
    {
        cout << LMSG << "avformat_find_stream_info ret:" << ret << ", err:" << GetAVError(ret) << endl;
        return ret;
    }

    for (unsigned int index = 0; index < avformat_ctx_->nb_streams; ++index)
    {
        if (avformat_ctx_->streams[index]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index_ = index;
        }
        else if (avformat_ctx_->streams[index]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_index_ = index;
        }
    }

    cout << LMSG << "success open media file:" << file << endl;
    
    return 0;
}

int MediaInput::ReadFrame(uint8_t*& data, int& size, int& flags, bool& is_video, uint64_t& timestamp)
{
	int ret = av_read_frame(avformat_ctx_, &packet_);
    if (ret < 0)
    {   
        if (ret == AVERROR_EOF)
        {   
            cout << LMSG << "end of file" << endl;
            return 0;
        }   
        else
        {   
            cout << LMSG << "av_read_frame ret:" << ret << ", err:" << GetAVError(ret) << endl;
        }   

        return -1; 
    }

    if (packet_.stream_index == video_stream_index_)
    {
        cout << LMSG << "read video" << endl;
        is_video = true;
    }
    else if (packet_.stream_index == audio_stream_index_)
    {
        cout << LMSG << "read audio" << endl;
        is_video = false;
    }

    data = packet_.data;
    size = packet_.size;
    flags = packet_.flags;
    timestamp = packet_.dts;

    return size;
}
