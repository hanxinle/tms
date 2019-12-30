#ifndef __SRT_MGR_H__
#define __SRT_MGR_H__

#include <map>
#include <string>

#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::string;

class IoLoop;
class Fd;
class SrtProtocol;

class SrtMgr : public SocketHandle, public TimerSecondHandle
{
public:
    SrtMgr(IoLoop* io_loop);
    ~SrtMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    SrtProtocol* GetOrCreateProtocol(Fd& socket);

private:
    IoLoop* io_loop_;
    map<int, SrtProtocol*> fd_protocol_;
};

#endif // __SRT_MGR_H__
