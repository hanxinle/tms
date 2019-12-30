#include "fd.h"
#include "web_socket_mgr.h"
#include "web_socket_protocol.h"

WebSocketMgr::WebSocketMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

WebSocketMgr::~WebSocketMgr()
{
}

int WebSocketMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	WebSocketProtocol* web_socket_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = web_socket_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }

    return ret;
}

int WebSocketMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	WebSocketProtocol* web_socket_protocol = GetOrCreateProtocol(socket);

    web_socket_protocol->OnStop();

    delete web_socket_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int WebSocketMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	WebSocketProtocol* web_socket_protocol = GetOrCreateProtocol(socket);

    web_socket_protocol->OnStop();

    delete web_socket_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int WebSocketMgr::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    return kSuccess;
}

WebSocketProtocol* WebSocketMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.fd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new WebSocketProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}
