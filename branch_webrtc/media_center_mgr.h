#ifndef __MEDIA_CENTER_MGR_H__
#define __MEDIA_CENTER_MGR_H__

#include <map>
#include <string>

#include "media_center_protocol.h"
#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::string;

class Epoller;
class Fd;
class MediaCenterProtocol;

class MediaCenterMgr : public SocketHandle, public TimerSecondHandle
{
public:
    MediaCenterMgr(Epoller* epoller);
    ~MediaCenterMgr();

    MediaCenterProtocol* GetOrCreateProtocol(Fd& socket);

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    int ConnectMediaCenter(const string& ip, const uint16_t& port);

    int GetAppStreamMasterNode(const string& app, const string& stream);

    int SendAll(const Rpc& rpc);

private:
    Epoller* epoller_;
    map<int, MediaCenterProtocol*> fd_protocol_;
};

#endif // __MEDIA_CENTER_MGR_H__
