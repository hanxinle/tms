#ifndef __HTTP_HLS_MGR_H__
#define __HTTP_HLS_MGR_H__

#include <map>

#include "socket_handle.h"

using std::map;

class IoLoop;
class Fd;
class HttpHlsProtocol;
class ServerMgr;

class HttpHlsMgr : public SocketHandle
{
public:
    HttpHlsMgr(IoLoop* io_loop);
    ~HttpHlsMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

private:
    HttpHlsProtocol* GetOrCreateProtocol(Fd& socket);

private:
    IoLoop* io_loop_;
    map<int, HttpHlsProtocol*> fd_protocol_;
};

#endif // __HTTP_HLS_MGR_H__
