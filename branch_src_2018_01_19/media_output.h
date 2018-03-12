#ifndef __MEDIA_OUTPUT_H__
#define __MEDIA_OUTPUT_H__

#include <string>
#include <vector>

#include "video_define.h"

extern "C"
{
#include "libavformat/avformat.h"
}

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

    int WriteAudio(AVPacket* packet);
    int WriteVideo(AVPacket* packet);

private:
    string file_name_;

    AVFormatContext* avformat_ctx_;
    AVStream* avstream_audio_;
    AVStream* avstream_video_;

    vector<MediaPacket> video_cache_;
    vector<MediaPacket> audio_cache_;
};

#endif // __MEDIA_OUTPUT_H__
