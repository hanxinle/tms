#ifndef __SERVER_PROTOCOL_H__
#define __SERVER_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <sstream>
#include <set>

#include "crc32.h"
#include "media_publisher.h"
#include "ref_ptr.h"
#include "socket_util.h"
#include "trace_tool.h"

using std::map;
using std::string;
using std::ostringstream;
using std::set;

class Epoller;
class Fd;
class HttpFlvProtocol;
class IoBuffer;
class RtmpProtocol;
class ServerMgr;
class TcpSocket;

class ServerProtocol : public MediaPublisher
{
public:
    ServerProtocol(Epoller* epoller, Fd* socket, ServerMgr* server_mgr);
    ~ServerProtocol();

    int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    TcpSocket* GetTcpSocket()
    {
        return (TcpSocket*)socket_;
    }

    MediaMuxer& GetMediaMuxer()
    {
        return media_muxer_;
    }

    int SendMediaData(const Payload& payload);
    int SendAudioHeader(const string& audio_header);
    int SendVideoHeader(const string& video_header);
    int SendMetaData(const string& meta_data);
    int SendStreamName();
    int SendAppName();

    void SetApp(const string& app)
    {
        app_ = app;
    }

    void SetStreamName(const string& stream_name)
    {
        stream_name_ = stream_name;
    }

    void SetMediaPublisher(MediaPublisher* media_publisher)
    {
        media_publisher_ = media_publisher;
    }

private:
    int OnNewRtmpPlayer(RtmpProtocol* protocol);
    int OnNewFlvPlayer(HttpFlvProtocol* protocol);

private:
    Epoller* epoller_;
    Fd* socket_;
    ServerMgr* server_mgr_;
    MediaPublisher* media_publisher_;

    string app_;
    string stream_name_;
};

#endif // __SERVER_PROTOCOL_H__
