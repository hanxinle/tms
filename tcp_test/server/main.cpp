#include <iostream>

#include "epoller.h"
#include "socket_util.h"
#include "tcp_socket.h"
#include "util.h"

using namespace std;
using namespace socket_util;

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

    Epoller epoller;

    int server_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_fd);
    Bind(server_fd, server_ip, server_port);
    Listen(server_fd);
    SetNonBlock(server_fd);

    TcpSocket server_socket(&epoller, server_fd, NULL);
    server_socket.EnableRead();
    server_socket.AsServerSocket();

    while (true)
    {
        epoller.Run();
    }

    return 0;
}
