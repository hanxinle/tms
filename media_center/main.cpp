#include <signal.h>

#include <iostream>

#include "epoller.h"
#include "media_node_discovery_mgr.h"
#include "media_node_mgr.h"
#include "protocol.h"
#include "socket_util.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"
#include "util.h"

using namespace std;
using namespace socket_util;

NodeInfo g_node_info;

int main(int argc, char* argv[])
{
	map<string, string> args_map = Util::ParseArgs(argc, argv);

    string server_ip = "";
    uint16_t server_port = 16002;
    bool daemon = false;

    auto iter_server_ip = args_map.find("server_ip");
    auto iter_server_port = args_map.find("server_port");
    auto iter_daemon = args_map.find("daemon");

    if (iter_server_ip == args_map.end())
    {
        cout << "Usage:" << argv[0] << " -server_ip <xxx.xxx.xxx.xxx> -server_port [xxx] -daemon [xxx]" << endl;
        return 0;
    }

    server_ip = iter_server_ip->second;

    if (iter_server_port != args_map.end())
    {
        if (! iter_server_port->second.empty())
        {
            server_port = Util::Str2Num<uint16_t>(iter_server_port->second);
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
    g_node_info.type = MEDIA_CENTER;
    g_node_info.start_time_ms = Util::GetNowMs();
    g_node_info.pid = getpid();

    Epoller epoller;

    int media_node_fd = CreateNonBlockTcpSocket();

    ReuseAddr(media_node_fd);
    Bind(media_node_fd, server_ip, server_port);
    Listen(media_node_fd);
    SetNonBlock(media_node_fd);

    MediaNodeMgr media_node_mgr(&epoller);

    TcpSocket media_node_socket(&epoller, media_node_fd, &media_node_mgr);
    media_node_socket.EnableRead();
    media_node_socket.AsServerSocket();
    // ================================================================

    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

	MediaNodeDiscoveryMgr media_node_discovery_mgr(&epoller);
    media_node_discovery_mgr.ConnectNodeDiscovery("127.0.0.1", 16001);

    timer_in_second.AddTimerSecondHandle(&media_node_mgr);
	timer_in_second.AddTimerSecondHandle(&media_node_discovery_mgr);

    while (true)
    {
        epoller.Run();
    }

    return 0;
}
