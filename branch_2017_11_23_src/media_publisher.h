#ifndef __MEDIA_PUBLISHER_H__
#define __MEDIA_PUBLISHER_H__

#include "media_muxer.h"

class HttpFlvProtocol;
class RtmpProtocol;
class ServerProtocol;

class MediaPublisher
{
public:
    bool AddForwardRtmpServer(RtmpProtocol* protocol)
    {   
        if (rtmp_forwards_.count(protocol))
        {   
            return false;
        }   

        rtmp_forwards_.insert(protocol);

        OnNewRtmpPlayer(protocol);

        return true;
    }

	bool RemoveForwardRtmpServer(RtmpProtocol* protocol)
    {   
        if (rtmp_forwards_.find(protocol) == rtmp_forwards_.end())
        {   
            return false;
        }   

        rtmp_forwards_.erase(protocol);

        return true;
    }   

    bool AddRtmpPlayer(RtmpProtocol* protocol)
    {   
        if (rtmp_player_.count(protocol))
        {   
            return false;
        }   

        rtmp_player_.insert(protocol);

        OnNewRtmpPlayer(protocol);

        return true;
    }   

    bool RemoveRtmpPlayer(RtmpProtocol* protocol)
    {   
        if (rtmp_player_.count(protocol) == 0)
        {   
            return false;
        }   

        rtmp_player_.erase(protocol);

        return true;
    }

    bool AddFlvPlayer(HttpFlvProtocol* protocol)
    {   
        if (flv_player_.count(protocol))
        {   
            return false;
        }   

        flv_player_.insert(protocol);

        OnNewFlvPlayer(protocol);
    
        return true;
    }   

	bool RemoveFlvPlayer(HttpFlvProtocol* protocol)
    {
        if (flv_player_.count(protocol) == 0)
        {
            return false;
        }   
        
        flv_player_.erase(protocol);
        
        return true;
    }   
    
    bool AddFollowServer(ServerProtocol* protocol)
    {
        if (server_follow_.count(protocol))
        {
            return false;
        }   
        
        server_follow_.insert(protocol);
        
        return true;
    }

    bool RemoveFollowServer(ServerProtocol* protocol)
    {
        if (server_follow_.count(protocol) == 0)
        {
            return false;
        }   
        
        server_follow_.erase(protocol);
        
        return true;
    }
	
	MediaMuxer& GetMediaMuxer()
    {   
        return media_muxer_;
    }

protected:
    int OnNewRtmpPlayer(RtmpProtocol* protocol);
    int OnNewFlvPlayer(HttpFlvProtocol* protocol);

protected:
	set<RtmpProtocol*> rtmp_forwards_;
    set<RtmpProtocol*> rtmp_player_;
    set<HttpFlvProtocol*> flv_player_;
    set<ServerProtocol*> server_follow_;

    MediaMuxer media_muxer_;
};

#endif // __MEDIA_PUBLISHER_H__
