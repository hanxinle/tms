#ifndef __TIMER_HANDLE_H__
#define __TIMER_HANDLE_H__

class TimerSecondHandle
{
public:
    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) = 0;
};

class TimerMillSecondHandle
{
public:
    virtual int HandleTimerInMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count) = 0;
};

#endif // __TIMER_HANDLE_H__
