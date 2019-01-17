#include "fd.h"
#include "udp_socket.h"
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
    auto iter = fd_protocol_.begin();
    while (iter != fd_protocol_.end())
    {   
        iter->second->EveryNSecond(now_in_ms, interval, count);

        bool can_close = iter->second->CheckCanClose();

        if (can_close)
        {
            delete iter->second;
            iter = fd_protocol_.erase(iter);
        }
        else
        {
            ++iter;
        }
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

void WebrtcMgr::__DebugBroadcast(const uint8_t* data, const int& len)
{
    for (auto& kv : fd_protocol_)
    {
        if (kv.second->DtlsHandshakeDone())
        {
            uint8_t protect_rtp[1500];
            int protect_rtp_len = len;
            if (kv.second->ProtectRtp(data, len, protect_rtp, protect_rtp_len) == 0)
            {
                kv.second->GetUdpSocket()->Send(protect_rtp, protect_rtp_len);
            }
        }
    }
}

void WebrtcMgr::__DebugSendH264(const uint8_t* data, const int& len, const uint32_t& dts)
{
    cout << LMSG << "h264 client count:" << fd_protocol_.size() << endl;
    for (auto& kv : fd_protocol_)
    {
        if (kv.second->DtlsHandshakeDone())
        {
            kv.second->SendH264Data(data, len, dts);
        }
    }
}
