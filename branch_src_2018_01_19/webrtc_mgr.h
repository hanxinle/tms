#ifndef __WEBRTC_MGR_H__
#define __WEBRTC_MGR_H__

#include <map>
#include <set>

#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::set;

class Epoller;
class Fd;
class WebrtcProtocol;

class WebrtcMgr : public SocketHandle, public TimerSecondHandle, public TimerMillSecondHandle
{
public:
    WebrtcMgr(Epoller* epoller);
    ~WebrtcMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

	virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);
    virtual int HandleTimerInMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    WebrtcProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    map<int, WebrtcProtocol*> fd_protocol_;
};

#endif // __WEBRTC_MGR_H__
