#ifndef __STREAM_MGR_H__
#define __STREAM_MGR_H__

#include <map>

#include "socket_handle.h"
#include "timer_handle.h"

using std::map;

class Epoller;
class Fd;
class RtmpProtocol;

class StreamMgr : public SocketHandle, public TimerSecondHandle
{
public:
    StreamMgr(Epoller* epoller);
    ~StreamMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    RtmpProtocol* GetOrCreateProtocol(Fd& socket);

    bool RegisterStream(const string& app, const string& stream_name, RtmpProtocol* rtmp_protocol);
    RtmpProtocol* GetRtmpProtocolByAppStream(const string& app, const string& stream_name);

private:
    Epoller* epoller_;
    map<int, RtmpProtocol*> fd_protocol_;

    map<string, map<string, RtmpProtocol*>> app_stream_protocol_;
};

#endif // __STREAM_MGR_H__
