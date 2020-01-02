#ifndef __SRT_PROTOCOL_H__
#define __SRT_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <string>

class IoLoop;
class Fd;
class IoBuffer;
class SrtSocket;

class SrtProtocol
{
public:
    SrtProtocol(IoLoop* io_loop, Fd* socket);
    ~SrtProtocol();

	int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);
    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) { return 0; }

    SrtSocket* GetSrtSocket()
    {
        return (SrtSocket*)socket_;
    }

private:
	IoLoop* io_loop_;
    Fd* socket_;
};

#endif // __SRT_PROTOCOL_H__
