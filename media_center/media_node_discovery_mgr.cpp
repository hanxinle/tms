#include "media_node_discovery_mgr.h"

#include "fd.h"
#include "socket_util.h"
#include "tcp_socket.h"

using namespace socket_util;

MediaNodeDiscoveryMgr::MediaNodeDiscoveryMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

MediaNodeDiscoveryMgr::~MediaNodeDiscoveryMgr()
{
}

MediaNodeDiscoveryProtocol* MediaNodeDiscoveryMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new MediaNodeDiscoveryProtocol(epoller_, &socket, this);
    }   

    return fd_protocol_[fd];
}

int MediaNodeDiscoveryMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	MediaNodeDiscoveryProtocol* media_node_discovery_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = media_node_discovery_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }   

    return ret;
}

int MediaNodeDiscoveryMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
	MediaNodeDiscoveryProtocol* media_node_discovery_protocol = GetOrCreateProtocol(socket);

    media_node_discovery_protocol->OnStop();

    delete media_node_discovery_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int MediaNodeDiscoveryMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
	MediaNodeDiscoveryProtocol* media_node_discovery_protocol = GetOrCreateProtocol(socket);

    media_node_discovery_protocol->OnStop();

    delete media_node_discovery_protocol;
    fd_protocol_.erase(socket.GetFd());
}

int MediaNodeDiscoveryMgr::HandleConnected(Fd& socket)
{
	MediaNodeDiscoveryProtocol* media_node_discovery_protocol = GetOrCreateProtocol(socket);

    media_node_discovery_protocol->OnConnected();
}

int MediaNodeDiscoveryMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (const auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }
}


int MediaNodeDiscoveryMgr::ConnectNodeDiscovery(const string& ip, const uint16_t& port)
{
	int media_node_discovery_fd = CreateNonBlockTcpSocket();
    int ret = ConnectHost(media_node_discovery_fd, ip, port);

    if (ret < 0 && errno != EINPROGRESS)
    {    
        cout << LMSG << "Connect ret:" << ret << endl;
        return -1; 
    }    

    Fd* socket = new TcpSocket(epoller_, media_node_discovery_fd, (SocketHandle*)this);

    if (errno == EINPROGRESS)
    {    
        ((TcpSocket*)socket)->SetConnecting();
        ((TcpSocket*)socket)->EnableWrite();
    }    
    else 
    {    
        ((TcpSocket*)socket)->SetConnected();
        ((TcpSocket*)socket)->EnableRead();
    }

	return 0;
}

int MediaNodeDiscoveryMgr::SendAll(const Rpc& rpc)
{
    if (fd_protocol_.empty())
    {   
        return -1; 
    }   

    for (auto& kv : fd_protocol_)
    {   
        kv.second->Send(rpc);
    }   

    return 0;
}
