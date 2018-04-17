#ifndef __MEDIA_INPUT_H__
#define __MEDIA_INPUT_H__

#include <string>

extern "C"
{
#include "libavformat/avformat.h"
};

using std::string;

class MediaInput
{
public:
    MediaInput();
    ~MediaInput();

    int Open(const string& file);
    int ReadFrame(uint8_t*& data, int& size, int& flags, bool& is_video, uint64_t& timestamp);

private:
    string file_name_;

    AVFormatContext* avformat_ctx_;
    AVPacket         packet_;

    int audio_stream_index_;
    int video_stream_index_;
};

#endif // __MEDIA_INPUT_H__
