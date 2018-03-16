#ifndef __MEDIA_OUTPUT_H__
#define __MEDIA_OUTPUT_H__

#include <list>
#include <string>
#include <vector>

#include "video_define.h"

extern "C"
{
#include "libavformat/avformat.h"
}

using std::list;
using std::string;
using std::vector;

class MediaOutput
{
public:
    MediaOutput();
    ~MediaOutput();

    int Init(const string& file_name);
    int OpenFile();
    int InitAudioStream(const AVCodecContext* audio_encode_ctx);
    int InitVideoStream(const AVCodecContext* audio_encode_ctx);

    int InitAudioStream();
    int InitVideoStream();

    int WriteAudio(AVPacket* packet);
    int WriteVideo(AVPacket* packet);

    int WriteMedia(const uint8_t* data, const int& len, const bool& is_audio, const int64_t& dts);

private:
    int InternalWriteMedia(AVPacket* av_packet, const bool& is_audio);
    int InternalWriteMedia(MediaPacket media_packet);

private:
    string file_name_;

    AVFormatContext* avformat_ctx_;
    AVStream* avstream_audio_;
    AVStream* avstream_video_;

    vector<MediaPacket> video_cache_;
    vector<MediaPacket> audio_cache_;

    list<MediaPacket> media_buffer_;

    bool use_buffer_;

    bool fake_dts_;
    int64_t audio_dts_;
    int64_t video_dts_;

    int video_fps_;
    int audio_fps_;
};

#endif // __MEDIA_OUTPUT_H__
