#ifndef __SRT_EVENT_FD_H__
#define __SRT_EVENT_FD_H__

#include "io_buffer.h"
#include "fd.h"

class IoLoop;
class SrtEpoller;

class SrtEventFd : public Fd
{
public:
    SrtEventFd(IoLoop* io_loop, const int& fd);
    ~SrtEventFd();

    virtual int OnRead();
    virtual int OnWrite();
    virtual int Send(const uint8_t* data, const size_t& len);

    void SetSrtEpoller(SrtEpoller* srt_epoller)
    {
        srt_epoller_ = srt_epoller;
    }

private:
    SrtEpoller* srt_epoller_;
};

#endif // __SRT_EVENT_FD_H__
