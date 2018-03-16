#include <signal.h>

#include <iostream>

#include "bit_buffer.h"
#include "bit_stream.h"
#include "epoller.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "ref_ptr.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"
#include "trace_tool.h"
#include "udp_socket.h"
#include "util.h"
#include "video_define.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

using namespace std;
using namespace socket_util;

static void sighandler(int sig_no)
{
    cout << LMSG << "sig:" << sig_no << endl;
	exit(0);
} 

NodeInfo                g_node_info;
Epoller*                g_epoll = NULL;
MediaCenterMgr*         g_media_center_mgr = NULL;
MediaNodeDiscoveryMgr*  g_media_node_discovery_mgr = NULL;
ServerMgr*              g_server_mgr = NULL;

string                  g_server_ip = "";

void AvLogCallback(void* ptr, int level, const char* fmt, va_list vl)
{
    UNUSED(ptr);
    UNUSED(level);

    vprintf(fmt, vl);
}

int main(int argc, char* argv[])
{
    av_register_all();
    avcodec_register_all();

    av_log_set_callback(AvLogCallback);
    av_log_set_level(AV_LOG_VERBOSE);

    // parse args
    map<string, string> args_map = Util::ParseArgs(argc, argv);

    uint16_t server_port            = 10001;
    bool daemon                     = false;

    auto iter_server_ip     = args_map.find("server_ip");
    auto iter_daemon        = args_map.find("daemon");

    if (iter_server_ip == args_map.end())
    {
        cout << "Usage:" << argv[0] << " -server_ip <xxx.xxx.xxx.xxx> -server_port [xxx] -daemon [xxx]" << endl;
        return 0;
    }

    g_server_ip = iter_server_ip->second;

    if (iter_daemon != args_map.end())
    {
        int tmp = Util::Str2Num<int>(iter_daemon->second);

        daemon = (! (tmp == 0));
    }

    if (daemon)
    {
        Util::Daemon();
    }

	IpStr2Num(g_server_ip, g_node_info.ip);
    g_node_info.port.push_back(server_port);
    g_node_info.type          = RTMP_NODE;
    g_node_info.start_time_ms = Util::GetNowMs();
    g_node_info.pid           = getpid();

	signal(SIGUSR1, sighandler);
    signal(SIGPIPE,SIG_IGN);

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
