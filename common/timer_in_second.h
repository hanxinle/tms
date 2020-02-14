#ifndef __TIMER_IN_SECOND_H__
#define __TIMER_IN_SECOND_H__

#include <set>

#include "fd.h"

class TimerSecondHandle;

class TimerInSecond : public Fd
{
public:
    TimerInSecond(IoLoop* io_loop);
    ~TimerInSecond();

    bool AddTimerSecondHandle(TimerSecondHandle* handle)
    {
        auto iter = second_handle_.insert(handle);

        return iter.second;
    }

    int RunEverySecond();

    int Send(const uint8_t* data, const size_t& len)
    {
        UNUSED(data);
        UNUSED(len);

        return 0;
    }

    int OnRead();
    int OnWrite() 
    { 
        return 0;
    }

private:
    std::set<TimerSecondHandle*> second_handle_;

    uint64_t now_in_ms_;
    uint64_t count_;
};

#endif // __TIMER_IN_SECOND_H__
