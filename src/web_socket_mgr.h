#ifndef __WEB_SOCKET_MGR_H__
#define __WEB_SOCKET_MGR_H__

#include <map>
#include <set>

#include "socket_handle.h"

using std::map;
using std::set;

class IoLoop;
class Fd;
class WebSocketProtocol;

class WebSocketMgr : public SocketHandle
{
public:
    WebSocketMgr(IoLoop* io_loop);
    ~WebSocketMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

private:
    WebSocketProtocol* GetOrCreateProtocol(Fd& socket);

private:
    IoLoop* io_loop_;
    map<int, WebSocketProtocol*> fd_protocol_;
};

#endif // __WEB_SOCKET_MGR_H__
