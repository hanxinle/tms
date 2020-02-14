#include <fcntl.h>
#include <unistd.h>

#include <iostream>

#include "protocol_mgr.h"
#include "srt_epoller.h"
#include "srt_socket_util.h"
#include "srt_socket.h"
#include "ts_reader.h"
#include "util.h"

ProtocolMgr<SrtProtocol>* g_srt_mgr = NULL;

using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "Usage " << argv[0] << " xxx.xxx.xxx.xxx port" << endl;
        return 0;
    }

    string ip = argv[1];
    uint16_t port = Util::Str2Num<uint16_t>(argv[2]);

	srt_startup();
    srt_setloglevel(srt_logging::LogLevel::note);

    SrtEpoller srt_epoller;
    srt_epoller.Create();

    int srt_fd = srt_socket_util::CreateSrtSocket();
    srt_socket_util::SetTransTypeLive(srt_fd);
    srt_socket_util::SetBlock(srt_fd, false);
    srt_socket_util::SetSendBufSize(srt_fd, 10*1024*1024);
    srt_socket_util::SetRecvBufSize(srt_fd, 10*1024*1024);
    srt_socket_util::SetPeerIdleTimeout(srt_fd, 20*60*1000);
    srt_socket_util::SetUdpSendBufSize(srt_fd, 10*1024*1024);
    srt_socket_util::SetUdpRecvBufSize(srt_fd, 10*1024*1024);
    srt_socket_util::SetLatency(srt_fd, 1000);

    ProtocolMgr<SrtProtocol> srt_mgr(&srt_epoller);
    g_srt_mgr = &srt_mgr;

    SrtSocket srt_socket(&srt_epoller, srt_fd, &srt_mgr);
    srt_socket.ConnectIp(ip, port);

    // Event Loop
    while (true)
    {   
        srt_epoller.WaitIO(100);
        cout << "wait io return" << endl;
    }   
  
    return 0;
}
