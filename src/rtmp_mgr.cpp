#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "fd.h"
#include "rtmp_mgr.h"

RtmpMgr::RtmpMgr(IoLoop* io_loop)
    :
    io_loop_(io_loop)
{
}

RtmpMgr::~RtmpMgr()
{
}

int RtmpMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = rtmp_protocol->Parse(io_buffer)) == kSuccess)
    {
    }

    return ret;
}

int RtmpMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    // 尝试把read_buffer里面的数据都读完
    while (rtmp_protocol->Parse(io_buffer) == kSuccess)
    {
    }

    rtmp_protocol->OnStop();

    delete rtmp_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int RtmpMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    // 尝试把read_buffer里面的数据都读完
    while (rtmp_protocol->Parse(io_buffer) == kSuccess)
    {
    }

    rtmp_protocol->OnStop();

    delete rtmp_protocol;
    fd_protocol_.erase(socket.fd());

    return kSuccess;
}

int RtmpMgr::HandleConnected(Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    rtmp_protocol->OnConnected();

    return kSuccess;
}

RtmpProtocol* RtmpMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.fd();
    if (fd_protocol_.count(fd) == 0)
    {
        fd_protocol_[fd] = new RtmpProtocol(io_loop_, &socket);
    }

    return fd_protocol_[fd];
}

int RtmpMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }

    return kSuccess;
}
