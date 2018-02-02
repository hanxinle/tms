#include "fd.h"
#include "echo_mgr.h"
#include "echo_protocol.h"

EchoMgr::EchoMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

EchoMgr::~EchoMgr()
{
}

int EchoMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
    cout << LMSG << endl;
	EchoProtocol* echo_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    ret = echo_protocol->Parse(io_buffer);

    return ret;
}

int EchoMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

    return kSuccess;
}

int EchoMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

    return kSuccess;
}

int EchoMgr::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    return kSuccess;
}

EchoProtocol* EchoMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new EchoProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}

int EchoMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {   
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }   

    return kSuccess;
}
