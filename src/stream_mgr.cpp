#include "rtmp_protocol.h"
#include "socket.h"
#include "stream_mgr.h"

StreamMgr::StreamMgr()
{
}

StreamMgr::~StreamMgr()
{
}

int StreamMgr::HandleRead(IoBuffer& io_buffer, Socket& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    while (rtmp_protocol->Parse(io_buffer) == kSuccess)
    {
    }
}

RtmpProtocol* StreamMgr::GetOrCreateProtocol(Socket& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {
        fd_protocol_[fd] = new RtmpProtocol(&socket);
    }

    return fd_protocol_[fd];
}
