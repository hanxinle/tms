#include "fd.h"
#include "http_file_mgr.h"
#include "http_file_protocol.h"

HttpFileMgr::HttpFileMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

HttpFileMgr::~HttpFileMgr()
{
}

int HttpFileMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	HttpFileProtocol* http_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = http_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }

    return ret;
}

int HttpFileMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	HttpFileProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int HttpFileMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	HttpFileProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int HttpFileMgr::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    return kSuccess;
}

HttpFileProtocol* HttpFileMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new HttpFileProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}
