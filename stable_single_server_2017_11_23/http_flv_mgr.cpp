#include "fd.h"
#include "http_flv_mgr.h"
#include "http_flv_protocol.h"
#include "stream_mgr.h"

HttpFlvMgr::HttpFlvMgr(Epoller* epoller, StreamMgr* stream_mgr)
    :
    epoller_(epoller),
    stream_mgr_(stream_mgr)
{
}

HttpFlvMgr::~HttpFlvMgr()
{
}

int HttpFlvMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	HttpFlvProtocol* http_protocol = GetOrCreateProtocol(socket);

    while (http_protocol->Parse(io_buffer) == kSuccess)
    {   
    }
}

int HttpFlvMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
	HttpFlvProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int HttpFlvMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
	HttpFlvProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int HttpFlvMgr::HandleConnected(Fd& socket)
{
}

HttpFlvProtocol* HttpFlvMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new HttpFlvProtocol(epoller_, &socket, this, stream_mgr_);
    }   

    return fd_protocol_[fd];
}
