#ifndef __SOCKET_HANDLE_H__
#define __SOCKET_HANDLE_H__

class IoBuffer;
class Socket;

class SocketHandle
{
public:
    virtual int HandleRead(IoBuffer& io_buffer, Socket& socket) = 0;
};

#endif // __SOCKET_HANDLE_H__
