#include "fd.h"
#include "http_hls_mgr.h"
#include "http_hls_protocol.h"

HttpHlsMgr::HttpHlsMgr(IoLoop* io_loop)
    : io_loop_(io_loop)
{
}

HttpHlsMgr::~HttpHlsMgr()
{
}

int HttpHlsMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	HttpHlsProtocol* http_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = http_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }

    return ret;
}

int HttpHlsMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	HttpHlsProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int HttpHlsMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	HttpHlsProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int HttpHlsMgr::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    return kSuccess;
}

HttpHlsProtocol* HttpHlsMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.fd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new HttpHlsProtocol(io_loop_, &socket);
    }   

    return fd_protocol_[fd];
}
