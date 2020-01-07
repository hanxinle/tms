#ifndef __PROTOCOL_MGR_H__
#define __PROTOCOL_MGR_H__

#include <unordered_map>

#include "fd.h"
#include "socket_handle.h"
#include "timer_handle.h"

#include "admin_protocol.h"
#include "echo_protocol.h"
#include "http_flv_protocol.h"
#include "http_file_protocol.h"
#include "http_hls_protocol.h"
#include "rtmp_protocol.h"
#include "srt_protocol.h"
#include "web_socket_protocol.h"

class IoLoop;

template<typename PROTOCOL>
class ProtocolMgr : public SocketHandle, public TimerSecondHandle
{
public:
    ProtocolMgr(IoLoop* io_loop)
        : io_loop_(io_loop)
    {
    }

    virtual ~ProtocolMgr() {}

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket)
    {
        PROTOCOL* protocol = GetOrCreateProtocol(socket);

        int ret = kClose;

        while ((ret = protocol->Parse(io_buffer)) == kSuccess);

        return ret;
    }

    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket)
    {
        PROTOCOL* protocol = GetOrCreateProtocol(socket);

        // 尝试把read_buffer里面的数据都读完
        while (protocol->Parse(io_buffer) == kSuccess);

        protocol->OnStop();

        delete protocol;
        fd_protocol_.erase(socket.fd());

        return kSuccess;
    }

    virtual int HandleError(IoBuffer& io_buffer, Fd& socket)
    {
        PROTOCOL* protocol = GetOrCreateProtocol(socket);

        // 尝试把read_buffer里面的数据都读完
        while (protocol->Parse(io_buffer) == kSuccess);

        protocol->OnStop();

        delete protocol;
        fd_protocol_.erase(socket.fd());

        return kSuccess;
    }

    virtual int HandleConnected(Fd& socket)
    {
        PROTOCOL* protocol = GetOrCreateProtocol(socket);
        protocol->OnConnected();
        return kSuccess;
    }

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
    {
        for (auto& kv : fd_protocol_)
        {
            kv.second->EveryNSecond(now_in_ms, interval, count);
        }

        return kSuccess;
    }

    virtual int HandleTimerInMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
    {
        for (auto& kv : fd_protocol_)
        {
            kv.second->EveryNMillSecond(now_in_ms, interval, count);
        }

        return kSuccess;
    }

    PROTOCOL* GetOrCreateProtocol(Fd& socket)
    {
        int fd = socket.fd();
        if (fd_protocol_.count(fd) == 0)
        {
            fd_protocol_[fd] = new PROTOCOL(io_loop_, &socket);
        }

        return fd_protocol_[fd];
    }

private:
    IoLoop* io_loop_;
    std::unordered_map<int, PROTOCOL*> fd_protocol_;
};

#endif // __PROTOCOL_MGR_H__
