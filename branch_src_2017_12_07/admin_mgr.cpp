#include "admin_mgr.h"

#include "fd.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "tcp_socket.h"

using namespace socket_util;

AdminMgr::AdminMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

AdminMgr::~AdminMgr()
{
}

AdminProtocol* AdminMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new AdminProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}

int AdminMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	AdminProtocol* admin_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = admin_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }   

    return ret;
}

int AdminMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	AdminProtocol* admin_protocol = GetOrCreateProtocol(socket);

    admin_protocol->OnStop();

    delete admin_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int AdminMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	AdminProtocol* admin_protocol = GetOrCreateProtocol(socket);

    admin_protocol->OnStop();

    delete admin_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int AdminMgr::HandleConnected(Fd& socket)
{
	AdminProtocol* admin_protocol = GetOrCreateProtocol(socket);

    admin_protocol->OnConnected();

    return kSuccess;
}

int AdminMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (const auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }

    return kSuccess;
}
