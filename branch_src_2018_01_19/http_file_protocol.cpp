#include <iostream>
#include <map>

#include "base_64.h"
#include "bit_buffer.h"
#include "common_define.h"
#include "global.h"
#include "http_file_protocol.h"
#include "io_buffer.h"
#include "tcp_socket.h"

using namespace std;

HttpFileProtocol::HttpFileProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket),
    upgrade_(false)
{
}

HttpFileProtocol::~HttpFileProtocol()
{
}

int HttpFileProtocol::Parse(IoBuffer& io_buffer)
{
    int ret = http_parse_.Decode(io_buffer);

    return ret;
}

int HttpFileProtocol::Send(const uint8_t* data, const size_t& len)
{
    return kSuccess;
}

int HttpFileProtocol::OnStop()
{
    return 0;
}

int HttpFileProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return 0;
}
