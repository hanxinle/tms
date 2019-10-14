#ifndef __MEDIA_SUBSCRIBER_H__
#define __MEDIA_SUBSCRIBER_H__

#include <string>

#include "common_define.h"
#include "media_publisher.h"

class Payload;

using std::string;

// 所有可能是接收者的Protocol都需要继承这个类
class MediaSubscriber
{
public:
    MediaSubscriber(const uint16_t& type)
        :
        type_(type),
        expired_time_ms_(0),
        publisher_(NULL)
    {
    }

    virtual ~MediaSubscriber()
    {
        if (publisher_ != NULL)
        {
            publisher_->RemoveSubscriber(this);
        }
    }

    void SetPublisher(MediaPublisher* publisher)
    {
        publisher_ = publisher;
    }

    uint16_t GetType() const
    {
        return type_;
    }

    bool IsRtmp()
    {
        return type_ == kRtmp;
    }

    bool IsTcpServer()
    {
        return type_ == kTcpServer;
    }

    bool IsHttpFlv()
    {
        return type_ == kHttpFlv;
    }

    bool IsHttpHls()
    {
        return type_ == kHttpHls;
    }

    virtual int SendVideoHeader(const string& header)
    {
        UNUSED(header);
        return 0;
    }

    virtual int SendAudioHeader(const string& header)
    {
        UNUSED(header);
        return 0;
    }

    virtual int SendMetaData(const string& metadata)
    {
        UNUSED(metadata);
        return 0;
    }

    virtual int SendMediaData(const Payload& payload)
    {
        UNUSED(payload);
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
