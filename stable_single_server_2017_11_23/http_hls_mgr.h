#ifndef __HTTP_HLS_MGR_H__
#define __HTTP_HLS_MGR_H__

#include <map>

#include "socket_handle.h"

using std::map;

class Epoller;
class Fd;
class HttpHlsProtocol;
class StreamMgr;

class HttpHlsMgr : public SocketHandle
{
public:
    HttpHlsMgr(Epoller* epoller, StreamMgr* stream_mgr);
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
    StreamMgr* stream_mgr_;
};

#endif // __HTTP_HLS_MGR_H__
