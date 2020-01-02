#include "global.h"
#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "socket_util.h"
#include "srt_protocol.h"
#include "srt_socket.h"

using namespace socket_util;

SrtProtocol::SrtProtocol(IoLoop* io_loop, Fd* socket)
    :
    io_loop_(io_loop),
    socket_(socket)
{
}

SrtProtocol::~SrtProtocol()
{
}

int SrtProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;
    int len = io_buffer.Read(data, io_buffer.Size());

    if (len > 0)
    {
        return kSuccess;
    }

    return kNoEnoughData;
}

int SrtProtocol::OnStop()
{
    return kSuccess;
}

int SrtProtocol::OnConnected()
{
	GetSrtSocket()->SetConnected();
    GetSrtSocket()->EnableRead();
    GetSrtSocket()->DisableWrite();

	return 0;
}

int SrtProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    UNUSED(now_in_ms);
    UNUSED(interval);
    UNUSED(count);

    return kSuccess;
}
