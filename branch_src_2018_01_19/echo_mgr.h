#ifndef __ECHO_MGR_H__
#define __ECHO_MGR_H__

#include <map>
#include <set>

#include "socket_handle.h"

using std::map;
using std::set;

class Epoller;
class Fd;
class EchoProtocol;

class EchoMgr : public SocketHandle
{
public:
    EchoMgr(Epoller* epoller);
    ~EchoMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

	virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    EchoProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    map<int, EchoProtocol*> fd_protocol_;
};

#endif // __ECHO_MGR_H__
