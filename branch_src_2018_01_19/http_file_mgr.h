#ifndef __HTTP_FILE_MGR_H__
#define __HTTP_FILE_MGR_H__

#include <map>
#include <set>

#include "socket_handle.h"

using std::map;
using std::set;

class Epoller;
class Fd;
class HttpFileProtocol;

class HttpFileMgr : public SocketHandle
{
public:
    HttpFileMgr(Epoller* epoller);
    ~HttpFileMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

private:
    HttpFileProtocol* GetOrCreateProtocol(Fd& socket);

private:
    Epoller* epoller_;
    map<int, HttpFileProtocol*> fd_protocol_;
};

#endif // __HTTP_FILE_MGR_H__
