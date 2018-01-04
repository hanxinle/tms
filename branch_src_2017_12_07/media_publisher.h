#ifndef __MEDIA_PUBLISHER_H__
#define __MEDIA_PUBLISHER_H__

#include "media_muxer.h"

class HttpFlvProtocol;
class MediaSubscriber;
class RtmpProtocol;
class ServerProtocol;

class MediaPublisher
{
public:
    MediaPublisher()
        :
        media_muxer_(this)
    {
    }

    ~MediaPublisher()
    {
    }

    MediaMuxer& GetMediaMuxer()
    {   
        return media_muxer_;
    }
    
    set<MediaSubscriber*> GetAndClearWaitHeaderSubscriber()
    {
        auto ret = wait_header_subscriber_;
        wait_header_subscriber_.clear();

        return ret;
    }


    bool AddSubscriber(MediaSubscriber* subscriber);
    bool RemoveSubscriber(MediaSubscriber* subscriber);

protected:
    int OnNewSubscriber(MediaSubscriber* subscriber);

protected:
	set<MediaSubscriber*> subscriber_;
    set<MediaSubscriber*> wait_header_subscriber_;

    MediaMuxer media_muxer_;
};

#endif // __MEDIA_PUBLISHER_H__
