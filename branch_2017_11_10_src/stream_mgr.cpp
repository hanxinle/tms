#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "fd.h"
#include "stream_mgr.h"

StreamMgr::StreamMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

StreamMgr::~StreamMgr()
{
}

int StreamMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = rtmp_protocol->Parse(io_buffer)) == kSuccess)
    {
    }

    return ret;
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

int StreamMgr::HandleConnected(Fd& socket)
{
    RtmpProtocol* rtmp_protocol = GetOrCreateProtocol(socket);

    rtmp_protocol->OnConnected();
}

RtmpProtocol* StreamMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {
        fd_protocol_[fd] = new RtmpProtocol(epoller_, &socket, this);
    }

    return fd_protocol_[fd];
}

bool StreamMgr::RegisterStream(const string& app, const string& stream_name, RtmpProtocol* rtmp_protocol)
{
    auto& stream_protocol_ = app_stream_protocol_[app];

    if (stream_protocol_.find(stream_name) != stream_protocol_.end())
    {
        cout << LMSG << "stream_name:" << stream_name << " already registered" << endl;
        return false;
    }

    stream_protocol_.insert(make_pair(stream_name, rtmp_protocol));
    cout << LMSG << "register app:" << app << ", stream_name:" << stream_name << endl;

    return true;
}

RtmpProtocol* StreamMgr::GetRtmpProtocolByAppStream(const string& app, const string& stream_name)
{
    auto iter_app = app_stream_protocol_.find(app);

    if (iter_app == app_stream_protocol_.end())
    {
        return NULL;
    }

    auto iter_stream = iter_app->second.find(stream_name);

    if (iter_stream == iter_app->second.end())
    {
        return NULL;
    }

    return iter_stream->second;
}

bool StreamMgr::IsAppStreamExist(const string& app, const string& stream_name)
{
    auto iter_app = app_stream_protocol_.find(app);

    if (iter_app == app_stream_protocol_.end())
    {
        return false;
    }

    auto iter_stream = iter_app->second.find(stream_name);

    if (iter_stream == iter_app->second.end())
    {
        return false;
    }

    return true;
}

int StreamMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }
}
