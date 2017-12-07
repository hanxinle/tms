#include "fd.h"
#include "server_mgr.h"

ServerMgr::ServerMgr(Epoller* epoller)
{
}

ServerMgr::~ServerMgr()
{
}

ServerProtocol* ServerMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new ServerProtocol(epoller_, &socket, this);
    }   

    return fd_protocol_[fd];
}

int ServerMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	ServerProtocol* server_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = server_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }   

    return ret;
}

int ServerMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
	ServerProtocol* server_protocol = GetOrCreateProtocol(socket);

    server_protocol->OnStop();

    delete server_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int ServerMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
	ServerProtocol* server_protocol = GetOrCreateProtocol(socket);

    server_protocol->OnStop();

    delete server_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int ServerMgr::HandleConnected(Fd& socket)
{
	ServerProtocol* server_protocol = GetOrCreateProtocol(socket);

    server_protocol->OnConnected();
}

int ServerMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (const auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }
}
