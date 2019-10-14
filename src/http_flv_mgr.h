#ifndef __HTTP_FLV_MGR_H__
#define __HTTP_FLV_MGR_H__

#include <map>
#include <set>

#include "socket_handle.h"

using std::map;
using std::set;

class Epoller;
class Fd;
class HttpFlvProtocol;
class ServerMgr;
class RtmpMgr;

class HttpFlvMgr : public SocketHandle
{
public:
    HttpFlvMgr(Epoller* epoller);
    ~HttpFlvMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

	virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    HttpFlvProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    map<int, HttpFlvProtocol*> fd_protocol_;
};

#endif // __HTTP_FLV_MGR_H__
