#ifndef __VIDEO_DEFINE_H__
#define __VIDEO_DEFINE_H__

#include <string>

using std::string;

enum MediaType
{
    MediaUnknown = -1,
    MediaAudio = 0,
    MediaVideo = 1,
};

struct MediaPacket
{
    MediaPacket(const uint8_t* data, const int& len, const int64_t& dts, const int64_t& pts, const int& flag, const int8_t& type)
        :
        data_((const char*)data, len),
        dts_(dts),
        pts_(pts),
        type_(type),
        flag_(flag)
    {
    }

    MediaPacket()
        :
        data_(),
        dts_(-1),
        pts_(-1),
        type_(MediaUnknown),
        flag_(0)
    {
    }

    bool IsVideo()
    {
        return type_ == MediaVideo;
    }

    bool IsAudio()
    {
        return type_ == MediaAudio;
    }

    string data_;
    int64_t dts_;
    int64_t pts_;
    int8_t type_;
    int flag_;
};

#endif // __VIDEO_DEFINE_H__
