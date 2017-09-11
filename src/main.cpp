#include <signal.h>

#include <iostream>

#include "any.h"
#include "bit_buffer.h"
#include "ref_ptr.h"
#include "stream_mgr.h"
#include "epoller.h"
#include "socket_util.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"

using namespace any;
using namespace std;
using namespace socket_util;

int main(int argc, char* argv[])
{
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
        Payload b((uint8_t*)malloc(1024), 1024);

        Payload bb(b);

        vector<Payload> vb;

        vb.push_back(b);
        vb.push_back(bb);
    }

    Epoller epoller;

    int server_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_fd);
    Bind(server_fd, "0.0.0.0", 1935);

    Listen(server_fd);

    StreamMgr stream_mgr;

    SetNonBlock(server_fd);

    TcpSocket server_socket(&epoller, server_fd, &stream_mgr);
    server_socket.EnableRead();
    server_socket.AsServerSocket();

    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

    timer_in_second.AddTimerSecondHandle(&stream_mgr);

    while (true)
    {
        epoller.Run();
    }

    return 0;
}
