#include "fd.h"
#include "webrtc_mgr.h"
#include "webrtc_protocol.h"

WebrtcMgr::WebrtcMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

WebrtcMgr::~WebrtcMgr()
{
}

int WebrtcMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	WebrtcProtocol* webrtc_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    ret = webrtc_protocol->Parse(io_buffer);

    return ret;
}

int WebrtcMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

    return kSuccess;
}

int WebrtcMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

    return kSuccess;
}

int WebrtcMgr::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    return kSuccess;
}

WebrtcProtocol* WebrtcMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new WebrtcProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}

int WebrtcMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {   
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }   

    return kSuccess;
}

int WebrtcMgr::HandleTimerInMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {   
        kv.second->EveryNMillSecond(now_in_ms, interval, count);
    }   

    return kSuccess;
}
