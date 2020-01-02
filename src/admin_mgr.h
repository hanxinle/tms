#ifndef __ADMIN_MGR_H__
#define __ADMIN_MGR_H__

#include <map>
#include <string>

#include "admin_protocol.h"
#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::string;

class IoLoop;
class Fd;
class AdminProtocol;

class AdminMgr : public SocketHandle, public TimerSecondHandle
{
public:
    AdminMgr(IoLoop* io_loop);
    ~AdminMgr();

    AdminProtocol* GetOrCreateProtocol(Fd& socket);

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    IoLoop* io_loop_;
    map<int, AdminProtocol*> fd_protocol_;
};

#endif // __ADMIN_MGR_H__
