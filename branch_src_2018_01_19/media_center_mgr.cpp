#include "media_center_mgr.h"

#include "fd.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "tcp_socket.h"

using namespace socket_util;

MediaCenterMgr::MediaCenterMgr(Epoller* epoller)
    :
    epoller_(epoller)
{
}

MediaCenterMgr::~MediaCenterMgr()
{
}

MediaCenterProtocol* MediaCenterMgr::GetOrCreateProtocol(Fd& socket)
{
    int fd = socket.GetFd();
    if (fd_protocol_.count(fd) == 0)
    {   
        fd_protocol_[fd] = new MediaCenterProtocol(epoller_, &socket);
    }   

    return fd_protocol_[fd];
}

int MediaCenterMgr::HandleRead(IoBuffer& io_buffer, Fd& socket)
{
	MediaCenterProtocol* media_center_protocol = GetOrCreateProtocol(socket);

    int ret = kClose;

    while ((ret = media_center_protocol->Parse(io_buffer)) == kSuccess)
    {   
    }   

    return ret;
}

int MediaCenterMgr::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	MediaCenterProtocol* media_center_protocol = GetOrCreateProtocol(socket);

    media_center_protocol->OnStop();

    delete media_center_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}


int MediaCenterMgr::HandleError(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);

	MediaCenterProtocol* media_center_protocol = GetOrCreateProtocol(socket);

    media_center_protocol->OnStop();

    delete media_center_protocol;
    fd_protocol_.erase(socket.GetFd());

    return kSuccess;
}

int MediaCenterMgr::HandleConnected(Fd& socket)
{
	MediaCenterProtocol* media_center_protocol = GetOrCreateProtocol(socket);

    media_center_protocol->OnConnected();

    return kSuccess;
}

int MediaCenterMgr::HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    for (const auto& kv : fd_protocol_)
    {
        kv.second->EveryNSecond(now_in_ms, interval, count);
    }

    return kSuccess;
}

int MediaCenterMgr::ConnectMediaCenter(const string& ip, const uint16_t& port)
{
	int media_center_fd = CreateNonBlockTcpSocket();
    int ret = ConnectHost(media_center_fd, ip, port);

    if (ret < 0 && errno != EINPROGRESS)
    {    
        cout << LMSG << "Connect ret:" << ret << endl;
        return -1; 
    }

	Fd* socket = new TcpSocket(epoller_, media_center_fd, (SocketHandle*)this);

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

    return kSuccess;
}

int MediaCenterMgr::GetAppStreamMasterNode(const string& app, const string& stream)
{
    GetAppStreamMasterNodeReq get_app_stream_master_node_req;

    get_app_stream_master_node_req.req_time = Util::GetNowMs();
    get_app_stream_master_node_req.app = app;
    get_app_stream_master_node_req.stream = stream;

    return SendAll(get_app_stream_master_node_req);
}

int MediaCenterMgr::SendAll(const Rpc& rpc)
{
    if (fd_protocol_.empty())
    {
        return -1;
    }

    for (auto& kv : fd_protocol_)
    {
        if (kv.second->Send(rpc))
        {

        }
    }

    return 0;
}
