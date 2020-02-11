#include <assert.h>

#include <iostream>

#include "common_define.h"
#include "socket_util.h"
#include "socket_handler.h"
#include "srt_epoller.h"
#include "srt_event_fd.h"

SrtEventFd::SrtEventFd(IoLoop* io_loop, const int& fd)
    : Fd(io_loop, fd)
    , srt_epoller_(NULL)
{
}

SrtEventFd::~SrtEventFd()
{
}

int SrtEventFd::OnRead()
{
    if (srt_epoller_ != NULL)
    {
        srt_epoller_->WaitIO(0);
    }

    return kSuccess;
}

int SrtEventFd::OnWrite()
{
    return kSuccess;
}

int SrtEventFd::Send(const uint8_t* data, const size_t& len)
{
    return kSuccess;
}
