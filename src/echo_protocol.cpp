#include <iostream>
#include <map>

#include "common_define.h"
#include "echo_protocol.h"
#include "io_buffer.h"
#include "udp_socket.h"

using namespace std;

EchoProtocol::EchoProtocol(IoLoop* io_loop, Fd* socket)
    :
    io_loop_(io_loop),
    socket_(socket)
{
}

EchoProtocol::~EchoProtocol()
{
}

int EchoProtocol::Parse(IoBuffer& io_buffer)
{
    cout << LMSG << endl;

    uint8_t* data = NULL;
    int len = io_buffer.Read(data, io_buffer.Size());

    if (len > 0)
    {
        cout << LMSG << Util::Bin2Hex(data, len) << endl;

        GetUdpSocket()->Send(data, len);
        return kSuccess;
    }

    return kNoEnoughData;
}

int EchoProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return kSuccess;
}
