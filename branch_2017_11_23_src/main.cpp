#include <signal.h>

#include <iostream>

#include "any.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "http_flv_mgr.h"
#include "http_hls_mgr.h"
#include "ref_ptr.h"
#include "stream_mgr.h"
#include "epoller.h"
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
	exit(0);
} 

int main(int argc, char* argv[])
{
	signal(SIGUSR1, sighandler);

    Log::SetLogLevel(kLevelDebug);

    DEBUG << "john debug log" << endl;

    signal(SIGPIPE,SIG_IGN);

    Int i(1);
    Double d(1.0);
    String str("1.0");

    vector<Any*> va = {&i, &d, &str};

    Vector v(va);

    map<string, Any*> ma =  {{"int", &i}, {"double", &d}, {"string", &str}, {"vector", &v}};

    Map m(ma);

    map<string, Any*> mma = {{"map", &m}};

    Map mm(mma);

    Any* any = &mm;

    cout << any->IsMap() << endl;
    cout << (((*(Map*)any)["map"])->ToMap())["string"] << endl;

    any = &v;

    cout << (*any)[0] << endl;

    uint8_t data[8] = {0xFF, 0xAE, 0x14, 0x34, 0xA5, 0x88, 0x27, 0x96};
    string bf((const char*)data);

    BitBuffer bit_buffer(bf);

    uint64_t bits;

    cout << "0x";
    for (size_t index = 0; index != 16; ++index)
    {
        bit_buffer.GetBits(1, bits);

        cout << bits;
    }

    cout << endl;

    {
        BitStream bit_stream;
        uint8_t buf[5] = {0xBE, 0xA0, 0x11, 0x78, 0xFF};

        // 00100001
        bit_stream.WriteBits(3, (uint8_t)1);
        bit_stream.WriteBits(2, (uint8_t)0);
        bit_stream.WriteBits(3, (uint8_t)1);
        bit_stream.WriteBytes(2, (uint64_t)0xFA);
        bit_stream.WriteData(5, buf);


        cout << "BitStream:" << bit_stream.GetData() << ",size:" << bit_stream.SizeInBytes() << endl;
        cout << Util::Bin2Hex(bit_stream.GetData(), bit_stream.SizeInBytes()) << endl;
        cout << TRACE << endl;
    }

    {
        Payload b((uint8_t*)malloc(1024), 1024);

        Payload bb(b);

        vector<Payload> vb;

        vb.push_back(b);
        vb.push_back(bb);
    }

    Epoller epoller;

    int server_rtmp_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_rtmp_fd);
    Bind(server_rtmp_fd, "0.0.0.0", 1935);
    Listen(server_rtmp_fd);
    SetNonBlock(server_rtmp_fd);

    int server_flv_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_flv_fd);
    Bind(server_flv_fd, "0.0.0.0", 8787);
    Listen(server_flv_fd);
    SetNonBlock(server_flv_fd);

    int server_hls_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_hls_fd);
    Bind(server_hls_fd, "0.0.0.0", 8788);
    Listen(server_hls_fd);
    SetNonBlock(server_hls_fd);

    StreamMgr stream_mgr(&epoller);

    TcpSocket server_rtmp_socket(&epoller, server_rtmp_fd, &stream_mgr);
    server_rtmp_socket.EnableRead();
    server_rtmp_socket.AsServerSocket();

    HttpFlvMgr http_flv_mgr(&epoller, &stream_mgr);

    TcpSocket server_flv_socket(&epoller, server_flv_fd, &http_flv_mgr);
    server_flv_socket.EnableRead();
    server_flv_socket.AsServerSocket();

    HttpHlsMgr http_hls_mgr(&epoller, &stream_mgr);

    TcpSocket server_hls_socket(&epoller, server_hls_fd, &http_hls_mgr);
    server_hls_socket.EnableRead();
    server_hls_socket.AsServerSocket();


    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

    timer_in_second.AddTimerSecondHandle(&stream_mgr);

    while (true)
    {
        epoller.Run();
    }

    return 0;
}
