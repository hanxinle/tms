#ifndef __SERVER_PROTOCOL_H__
#define __SERVER_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <sstream>
#include <set>

#include "crc32.h"
#include "media_publisher.h"
#include "media_subscriber.h"
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

enum class ServerRole
{
    kUnknownServerRole = -1,

    kServerPush = 0,
    kPushServer = 1,
    kPullServer = 2,
};

class ServerProtocol : public MediaPublisher, public MediaSubscriber
{
public:
    ServerProtocol(Epoller* epoller, Fd* socket);
    ~ServerProtocol();

    int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();
    int OnAccept();

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
    int SendPullAppStream();

    void SetApp(const string& app)
    {
        app_ = app;
        media_muxer_.SetApp(app);
    }

    void SetStreamName(const string& stream)
    {
        stream_ = stream;
        media_muxer_.SetStreamName(stream);
    }

    void SetMediaPublisher(MediaPublisher* media_publisher)
    {
        media_publisher_ = media_publisher;
    }

    void SetServerPush()
    {
        role_ = ServerRole::kServerPush;
    }

    void SetPushServer()
    {
        role_ = ServerRole::kPushServer;
    }

    void SetPullServer()
    {
        role_ = ServerRole::kPullServer;
    }

private:
    int OnNewRtmpPlayer(RtmpProtocol* protocol);
    int OnNewFlvPlayer(HttpFlvProtocol* protocol);

private:
    Epoller* epoller_;
    Fd* socket_;
    MediaPublisher* media_publisher_;

    string app_;
    string stream_;

    ServerRole role_;
};

#endif // __SERVER_PROTOCOL_H__
