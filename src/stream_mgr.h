#ifndef __STREAM_MGR_H__
#define __STREAM_MGR_H__

#include <map>

#include "socket_handle.h"
#include "timer_handle.h"

using std::map;

class Fd;
class RtmpProtocol;

class StreamMgr : public SocketHandle, public TimerSecondHandle
{
public:
    StreamMgr();
    ~StreamMgr();

    virtual int HandleRead(IoBuffer& io_buffer, Fd& fd);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& fd);
    virtual int HandleError(IoBuffer& io_buffer, Fd& fd);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

private:
    RtmpProtocol* GetOrCreateProtocol(Fd& fd);

private:

    map<int, RtmpProtocol*> fd_protocol_;
};

#endif // __STREAM_MGR_H__
