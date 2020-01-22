#ifndef __MEDIA_SUBSCRIBER_H__
#define __MEDIA_SUBSCRIBER_H__

#include <string>

#include "common_define.h"
#include "media_publisher.h"

class Payload;

// 所有可能是接收者的Protocol都需要继承这个类
class MediaSubscriber
{
public:
    MediaSubscriber(const uint16_t& type)
        : type_(type)
        , expired_time_ms_(0)
        , publisher_(NULL)
    {
    }

    virtual ~MediaSubscriber()
    {
        if (publisher_ != NULL)
        {
            publisher_->RemoveSubscriber(this);
        }
    }

    void SetPublisher(MediaPublisher* publisher) { publisher_ = publisher; }

    uint16_t GetType() const { return type_; }

    bool IsRtmp() const { return type_ == kRtmp; }
    bool IsHttpFlv() const { return type_ == kHttpFlv; }
    bool IsHttpHls() const { return type_ == kHttpHls; }
    bool IsSrt() const { return type_ == kSrt; }
    bool IsWebrtc() const { return type_ == kWebrtc; }

    virtual int SendVideoHeader(const std::string& header)
    {
        UNUSED(header);
        return 0;
    }

    virtual int SendAudioHeader(const std::string& header)
    {
        UNUSED(header);
        return 0;
    }

    virtual int SendMetaData(const std::string& metadata)
    {
        UNUSED(metadata);
        return 0;
    }

    virtual int SendMediaData(const Payload& payload)
    {
        UNUSED(payload);
        return 0;
    }

    virtual int SendData(const std::string& data)
    {
        UNUSED(data);
        return 0;
    }

    virtual int OnStop()
    {
        return 0;
    }

protected:
    uint16_t type_;
    uint64_t expired_time_ms_;
    MediaPublisher* publisher_;
};

#endif // __MEDIA_SUBSCRIBER_H__
