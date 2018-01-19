#ifndef __SOCKET_HANDLE_H__
#define __SOCKET_HANDLE_H__

#include "common_define.h"

class Fd;
class IoBuffer;

class SocketHandle
{
public:
    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket) = 0;
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket) = 0;
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket) = 0;
    virtual int HandleConnected(Fd& socket) = 0;
    virtual int HandleAccept(Fd& socket) 
    {
        UNUSED(socket);

        return 0;
    }
};

#endif // __SOCKET_HANDLE_H__
