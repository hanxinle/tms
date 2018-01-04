#include <signal.h>

#include <iostream>

#include "epoller.h"
#include "media_node_mgr.h"
#include "socket_util.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"
#include "util.h"

using namespace std;
using namespace socket_util;

int main(int argc, char* argv[])
{
	map<string, string> args_map = Util::ParseArgs(argc, argv);

    uint16_t server_port = 16001;
    bool daemon = false;

    auto iter_server_port = args_map.find("server_port");
    auto iter_daemon = args_map.find("daemon");

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

    Epoller epoller;

    int media_node_discovery_fd = CreateNonBlockTcpSocket();

    ReuseAddr(media_node_discovery_fd);
    Bind(media_node_discovery_fd, "0.0.0.0", server_port);
    Listen(media_node_discovery_fd);
    SetNonBlock(media_node_discovery_fd);

    MediaNodeMgr media_node_mgr(&epoller);

    TcpSocket media_node_discovery_socket(&epoller, media_node_discovery_fd, &media_node_mgr);
    media_node_discovery_socket.EnableRead();
    media_node_discovery_socket.AsServerSocket();
    // ================================================================

    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

    timer_in_second.AddTimerSecondHandle(&media_node_mgr);

    while (true)
    {
        epoller.Run();
    }

    return 0;
}
