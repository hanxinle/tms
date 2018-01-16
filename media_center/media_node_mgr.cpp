#include "fd.h"
#include "media_node_mgr.h"

MediaNodeMgr::MediaNodeMgr(Epoller* epoller)
{
}

MediaNodeMgr::~MediaNodeMgr()
{
}

MediaNodeProtocol* MediaNodeMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new MediaNodeProtocol(epoller_, &socket, this);
    }   

    return fd_protocol_[fd];
}

int MediaNodeMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	MediaNodeProtocol* media_node_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = media_node_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }   

    return ret;
}

int MediaNodeMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
	MediaNodeProtocol* media_node_protocol = GetOrCreateProtocol(socket);

    media_node_protocol->OnStop();

    delete media_node_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int MediaNodeMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
	MediaNodeProtocol* media_node_protocol = GetOrCreateProtocol(socket);

    media_node_protocol->OnStop();

    delete media_node_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int MediaNodeMgr::HandleConnected(Fd& socket)
{
	MediaNodeProtocol* media_node_protocol = GetOrCreateProtocol(socket);

    media_node_protocol->OnConnected();
}

int MediaNodeMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (const auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }
}

bool MediaNodeMgr::InsertNodeAppStream(const NodeInfo& node_info, const StreamInfo& stream_info, const uint16_t& role)
{
    auto iter = node_app_stream_[node_info][stream_info.app].find(stream_info.stream);

    if (iter != node_app_stream_[node_info][stream_info.app].end())
    {
        return false;
    }

    node_app_stream_[node_info][stream_info.app].insert(stream_info.stream);
    app_stream_node_[stream_info.app][stream_info.stream].insert(node_info);

    if (role == MASTER)
    {
        app_stream_master_[stream_info.app][stream_info.stream] = node_info;
    }

    ostringstream os;

    node_info.Dump(os);

    cout << LMSG << "insert " << os.str() << ",app:" << stream_info.app << ",stream:" << stream_info.stream << endl;

    return true;
}

bool MediaNodeMgr::GetAppStreamMasterNode(const string& app, const string& stream, NodeInfo& node)
{
    auto iter_app = app_stream_master_.find(app);

    if (iter_app == app_stream_master_.end())
    {
        return false;
    }

    auto iter_stream = iter_app->second.find(stream);

    if (iter_stream == iter_app->second.end())
    {
        return false;
    }

    node = iter_stream->second;

    return true;
}
