#ifndef __MEDIA_SUBSCRIBER_H__
#define __MEDIA_SUBSCRIBER_H__

#include <string>

class Payload;

using std::string;

class MediaSubscriber
{
public:
    MediaSubscriber()
        :
        expired_time_ms_(0)
    {
    }

    ~MediaSubscriber()
    {
    }

    virtual int OnPendingArrive()
    {
        return 0;
    }
    
    virtual int SendVideoHeader(const string& header)
    {
        return 0;
    }

    virtual int SendAudioHeader(const string& header)
    {
        return 0;
    }

    virtual int SendMetaData(const string& metadata)
    {
        return 0;
    }

    virtual int SendMediaData(const Payload& payload)
    {
        return 0;
    }

    virtual int OnStop()
    {
        return 0;
    }

protected:
    uint64_t expired_time_ms_;
};

#endif // __MEDIA_SUBSCRIBER_H__
