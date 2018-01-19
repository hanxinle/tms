#ifndef __HTTP_HLS_MGR_H__
#define __HTTP_HLS_MGR_H__

#include <map>

#include "socket_handle.h"

using std::map;

class Epoller;
class Fd;
class HttpHlsProtocol;
class ServerMgr;
class RtmpMgr;

class HttpHlsMgr : public SocketHandle
{
public:
    HttpHlsMgr(Epoller* epoller, RtmpMgr* stream_mgr, ServerMgr* server_mgr);
    ~HttpHlsMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

private:
    HttpHlsProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    map<int, HttpHlsProtocol*> fd_protocol_;
    RtmpMgr* rtmp_mgr_;
    ServerMgr* server_mgr_;
};

#endif // __HTTP_HLS_MGR_H__
