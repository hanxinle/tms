#include "rtmp_protocol.h"
#include "fd.h"
#include "stream_mgr.h"

StreamMgr::StreamMgr()
{
}

StreamMgr::~StreamMgr()
{
}

int StreamMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    while (rtmp_protocol->Parse(io_buffer) == kSuccess)
    {
    }
}

int StreamMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    // 尝试把read_buffer里面的数据都读完
    while (rtmp_protocol->Parse(io_buffer) == kSuccess)
    {
    }

    rtmp_protocol->OnStop();

    delete rtmp_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int StreamMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    // 尝试把read_buffer里面的数据都读完
    while (rtmp_protocol->Parse(io_buffer) == kSuccess)
    {
    }

    rtmp_protocol->OnStop();

    delete rtmp_protocol;
    fd_protocol_.erase(socket.GetFd());
}

RtmpProtocol* StreamMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {
        fd_protocol_[fd] = new RtmpProtocol(&socket);
    }

    return fd_protocol_[fd];
}

int StreamMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }
}
