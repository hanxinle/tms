#include <signal.h>


#include <iostream>

#include "admin_mgr.h"
#include "any.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "epoller.h"
#include "http_flv_mgr.h"
#include "http_hls_mgr.h"
#include "local_stream_center.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "ref_ptr.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"
#include "trace_tool.h"
#include "util.h"

using namespace any;
using namespace std;
using namespace socket_util;

static void sighandler(int sig_no)
{
    cout << LMSG << "sig:" << sig_no << endl;
	exit(0);
} 

LocalStreamCenter       g_local_stream_center;
NodeInfo                g_node_info;
Epoller*                g_epoll = NULL;
HttpFlvMgr*             g_http_flv_mgr = NULL;
HttpHlsMgr*             g_http_hls_mgr = NULL;
MediaCenterMgr*         g_media_center_mgr = NULL;
MediaNodeDiscoveryMgr*  g_media_node_discovery_mgr = NULL;
RtmpMgr*                g_rtmp_mgr = NULL;
ServerMgr*              g_server_mgr = NULL;

int main(int argc, char* argv[])
{
    map<string, string> args_map = Util::ParseArgs(argc, argv);

    uint16_t rtmp_port     = 1935;
    uint16_t http_flv_port = 8787;
    uint16_t http_hls_port = 8788;
    string server_ip       = "";
    uint16_t server_port   = 10001;
    uint16_t admin_port    = 11000;
    bool daemon            = false;

    auto iter_server_ip     = args_map.find("server_ip");
    auto iter_rtmp_port     = args_map.find("rtmp_port");
    auto iter_http_flv_port = args_map.find("http_flv_port");
    auto iter_http_hls_port = args_map.find("http_hls_port");
    auto iter_server_port   = args_map.find("server_port");
    auto iter_admin_port    = args_map.find("admin_port");
    auto iter_daemon        = args_map.find("daemon");

    if (iter_server_ip == args_map.end())
    {
        cout << "Usage:" << argv[0] << " -server_ip <xxx.xxx.xxx.xxx> -server_port [xxx] -http_flv_port [xxx] -http_hls_port [xxx] -daemon [xxx]" << endl;
        return 0;
    }

    server_ip = iter_server_ip->second;

    if (iter_rtmp_port != args_map.end())
    {
        if (! iter_rtmp_port->second.empty())
        {
            rtmp_port = Util::Str2Num<uint16_t>(iter_rtmp_port->second);
        }
    }

    if (iter_http_flv_port != args_map.end())
    {
        if (! iter_http_flv_port->second.empty())
        {
            http_flv_port = Util::Str2Num<uint16_t>(iter_http_flv_port->second);
        }
    }

    if (iter_http_hls_port != args_map.end())
    {
        if (! iter_http_hls_port->second.empty())
        {
            http_hls_port = Util::Str2Num<uint16_t>(iter_http_hls_port->second);
        }
    }

    if (iter_server_port != args_map.end())
    {
        if (! iter_server_port->second.empty())
        {
            server_port = Util::Str2Num<uint16_t>(iter_server_port->second);
        }
    }

    if (iter_admin_port != args_map.end())
    {
        if (! iter_admin_port->second.empty())
        {
            admin_port = Util::Str2Num<uint16_t>(iter_admin_port->second);
        }
    }

    if (iter_daemon != args_map.end())
    {
        int tmp = Util::Str2Num<int>(iter_daemon->second);

        daemon = (! (tmp == 0));
    }

    if (daemon)
    {
        Util::Daemon();
    }

	IpStr2Num(server_ip, g_node_info.ip);
    g_node_info.port.push_back(server_port);
    g_node_info.type          = RTMP_NODE;
    g_node_info.start_time_ms = Util::GetNowMs();
    g_node_info.pid           = getpid();

	signal(SIGUSR1, sighandler);
    signal(SIGPIPE,SIG_IGN);

    Log::SetLogLevel(kLevelDebug);

    DEBUG << argv[0] << " starting..." << endl;

    Epoller epoller;
    g_epoll = &epoller;

    // === Init Timer ===
    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

    // === Init Server Stream Socket ===
    int server_stream_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_stream_fd);
    Bind(server_stream_fd, "0.0.0.0", server_port);
    Listen(server_stream_fd);
    SetNonBlock(server_stream_fd);

    ServerMgr server_mgr(&epoller);
    timer_in_second.AddTimerSecondHandle(&server_mgr);
    g_server_mgr = &server_mgr;

    TcpSocket server_stream_socket(&epoller, server_stream_fd, &server_mgr);
    server_stream_socket.EnableRead();
    server_stream_socket.AsServerSocket();

    // === Init Server Rtmp Socket ===
    int server_rtmp_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_rtmp_fd);
    Bind(server_rtmp_fd, "0.0.0.0", rtmp_port);
    Listen(server_rtmp_fd);
    SetNonBlock(server_rtmp_fd);

    RtmpMgr rtmp_mgr(&epoller, &server_mgr);
    timer_in_second.AddTimerSecondHandle(&rtmp_mgr);
    g_rtmp_mgr = &rtmp_mgr;

    TcpSocket server_rtmp_socket(&epoller, server_rtmp_fd, &rtmp_mgr);
    server_rtmp_socket.EnableRead();
    server_rtmp_socket.AsServerSocket();

    // === Init Server Flv Socket ===
    int server_flv_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_flv_fd);
    Bind(server_flv_fd, "0.0.0.0", http_flv_port);
    Listen(server_flv_fd);
    SetNonBlock(server_flv_fd);

    HttpFlvMgr http_flv_mgr(&epoller, &rtmp_mgr, &server_mgr);

    g_http_flv_mgr = &http_flv_mgr;

    TcpSocket server_flv_socket(&epoller, server_flv_fd, &http_flv_mgr);
    server_flv_socket.EnableRead();
    server_flv_socket.AsServerSocket();

    // === Init Server Hls Socket ===
    int server_hls_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_hls_fd);
    Bind(server_hls_fd, "0.0.0.0", http_hls_port);
    Listen(server_hls_fd);
    SetNonBlock(server_hls_fd);

    HttpHlsMgr http_hls_mgr(&epoller, &rtmp_mgr, &server_mgr);

    g_http_hls_mgr = &http_hls_mgr;

    TcpSocket server_hls_socket(&epoller, server_hls_fd, &http_hls_mgr);
    server_hls_socket.EnableRead();
    server_hls_socket.AsServerSocket();

    // === Init Admin Socket ===
    int admin_fd = CreateNonBlockTcpSocket();

    ReuseAddr(admin_fd);
    Bind(admin_fd, "0.0.0.0", admin_port);
    Listen(admin_fd);
    SetNonBlock(admin_fd);

    AdminMgr admin_mgr(&epoller);

    TcpSocket admin_socket(&epoller, admin_fd, &admin_mgr);
    admin_socket.EnableRead();
    admin_socket.AsServerSocket();

    // === Init Media Center ===
    MediaCenterMgr media_center_mgr(&epoller);
    timer_in_second.AddTimerSecondHandle(&media_center_mgr);
    g_media_center_mgr = &media_center_mgr;

    // === Init Media Node Discovery ===
    MediaNodeDiscoveryMgr media_node_discovery_mgr(&epoller);
    g_media_node_discovery_mgr = &media_node_discovery_mgr;
    media_node_discovery_mgr.ConnectNodeDiscovery("127.0.0.1", 16001);

    timer_in_second.AddTimerSecondHandle(&media_node_discovery_mgr);


    // Event Loop
    while (true)
    {
        epoller.Run();
    }

    return 0;
}
