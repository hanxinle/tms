#include "io_buffer.h"
#include "srt_protocol.h"
#include "fd.h"
#include "srt_mgr.h"

SrtMgr::SrtMgr(IoLoop* io_loop)
    :
    io_loop_(io_loop)
{
}

SrtMgr::~SrtMgr()
{
}

int SrtMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
    SrtProtocol* srt_protocol = GetOrCreateProtocol(socket);
    int ret = srt_protocol->Parse(io_buffer);
    return ret;
}

int SrtMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    SrtProtocol* srt_protocol = GetOrCreateProtocol(socket);

    // 尝试把read_buffer里面的数据都读完
    while (srt_protocol->Parse(io_buffer) == kSuccess)
    {
    }

    srt_protocol->OnStop();

    delete srt_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int SrtMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    SrtProtocol* srt_protocol = GetOrCreateProtocol(socket);

    // 尝试把read_buffer里面的数据都读完
    while (srt_protocol->Parse(io_buffer) == kSuccess)
    {
    }

    srt_protocol->OnStop();

    delete srt_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int SrtMgr::HandleConnected(Fd& socket)
{
    SrtProtocol* srt_protocol = GetOrCreateProtocol(socket);

    srt_protocol->OnConnected();

    return kSuccess;
}

SrtProtocol* SrtMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.fd();
    if (fd_protocol_.count(fd) == 0)
    {
        fd_protocol_[fd] = new SrtProtocol(io_loop_, &socket);
    }

    return fd_protocol_[fd];
}

int SrtMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }

    return kSuccess;
}
