#ifndef __STREAM_MGR_H__
#define __STREAM_MGR_H__

#include <map>

#include "socket_handle.h"

using std::map;

class RtmpProtocol;
class Socket;

class StreamMgr : public SocketHandle
{
public:
    StreamMgr();
    ~StreamMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Socket& socket);

private:
    RtmpProtocol* GetOrCreateProtocol(Socket& socket);

private:

    map<int, RtmpProtocol*> fd_protocol_;
};

#endif // __STREAM_MGR_H__
