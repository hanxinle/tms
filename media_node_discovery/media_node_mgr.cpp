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

bool MediaNodeMgr::InsertNode(const NodeInfo& node_info)
{
    return type_nodes_[node_info.type].insert(node_info).second;
}

set<NodeInfo> MediaNodeMgr::GetNode(const uint32_t& type, const uint16_t& num)
{
    set<NodeInfo> nodes;

    auto iter = type_nodes_.find(type);

    if (iter != type_nodes_.end())
    {
        cout << LMSG << endl;
        uint16_t count = 0;
        for (auto iter_node = iter->second.begin(); iter_node != iter->second.end() && count < num; ++iter_node,++count)
        {
            cout << LMSG << endl;
            nodes.insert(*iter_node);
        }
    }

    return nodes;
}
