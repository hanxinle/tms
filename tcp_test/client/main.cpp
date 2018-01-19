#include <iostream>

#include "socket_util.h"
#include "util.h"

using namespace std;
using namespace socket_util;

int main(int argc, char* argv[])
{
	map<string, string> args_map = Util::ParseArgs(argc, argv);

    string server_ip = "";
    uint16_t server_port = 16002;

    auto iter_server_ip = args_map.find("server_ip");
    auto iter_server_port = args_map.find("server_port");

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

    int client_fd = CreateTcpSocket();

    int ret = Connect(client_fd, server_ip, server_port);

    if (ret < 0)
    {
        cout << "connect err:" << ret << endl;
        return -1;
    }

    shutdown(client_fd, SHUT_WR);
    sleep(5);

    shutdown(client_fd, SHUT_RD);
    sleep(5);

    return 0;
}
