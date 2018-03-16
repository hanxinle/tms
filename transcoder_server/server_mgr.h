#ifndef __SERVER_MGR_H__
#define __SERVER_MGR_H__

#include <map>
#include <string>

#include "server_protocol.h"
#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::string;

class Epoller;
class Fd;
class ServerProtocol;

class ServerMgr : public SocketHandle, public TimerSecondHandle
{
public:
    ServerMgr(Epoller* epoller);
    ~ServerMgr();

    ServerProtocol* GetOrCreateProtocol(Fd& socket);

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);
    virtual int HandleAccept(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    Epoller* epoller_;
    map<int, ServerProtocol*> fd_protocol_;
};

#endif // __SERVER_MGR_H__
