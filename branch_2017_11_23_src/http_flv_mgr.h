#ifndef __HTTP_FLV_MGR_H__
#define __HTTP_FLV_MGR_H__

#include <map>

#include "socket_handle.h"

using std::map;

class Epoller;
class Fd;
class HttpFlvProtocol;
class ServerMgr;
class RtmpMgr;

class HttpFlvMgr : public SocketHandle
{
public:
    HttpFlvMgr(Epoller* epoller, RtmpMgr* stream_mgr, ServerMgr* server_mgr);
    ~HttpFlvMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

private:
    HttpFlvProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    map<int, HttpFlvProtocol*> fd_protocol_;
    RtmpMgr* rtmp_mgr_;
    ServerMgr* server_mgr_;
};

#endif // __HTTP_FLV_MGR_H__
