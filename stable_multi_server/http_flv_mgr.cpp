#include "fd.h"
#include "http_flv_mgr.h"
#include "http_flv_protocol.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"

HttpFlvMgr::HttpFlvMgr(Epoller* epoller, RtmpMgr* rtmp_mgr, ServerMgr* server_mgr)
    :
    epoller_(epoller),
    rtmp_mgr_(rtmp_mgr),
    server_mgr_(server_mgr)
{
}

HttpFlvMgr::~HttpFlvMgr()
{
}

int HttpFlvMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	HttpFlvProtocol* http_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = http_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }

    return ret;
}

int HttpFlvMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	HttpFlvProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int HttpFlvMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	HttpFlvProtocol* http_protocol = GetOrCreateProtocol(socket);

    http_protocol->OnStop();

    delete http_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int HttpFlvMgr::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    return kSuccess;
}

HttpFlvProtocol* HttpFlvMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new HttpFlvProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}
