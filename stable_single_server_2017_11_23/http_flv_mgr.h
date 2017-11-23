#ifndef __HTTP_FLV_MGR_H__
#define __HTTP_FLV_MGR_H__

#include <map>

#include "socket_handle.h"

using std::map;

class Epoller;
class Fd;
class HttpFlvProtocol;
class StreamMgr;

class HttpFlvMgr : public SocketHandle
{
public:
    HttpFlvMgr(Epoller* epoller, StreamMgr* stream_mgr);
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
    StreamMgr* stream_mgr_;
};

#endif // __HTTP_FLV_MGR_H__
