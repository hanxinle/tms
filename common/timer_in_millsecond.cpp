#include <sys/timerfd.h>

#include <string.h>

#include "timer_in_millsecond.h"
#include "timer_handle.h"
#include "util.h"

TimerInMillSecond::TimerInMillSecond(IoLoop* io_loop)
    :
    Fd(io_loop, timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
    now_in_ms_(Util::GetNowMs()),
    count_(1)
{
    if (fd_ == -1)
    {
        cout << LMSG << "timerfd_create err:" << strerror(errno) << endl;
    }

    itimerspec second_value;

	second_value.it_value.tv_sec = 0; // 表示过多少秒之后开始
    second_value.it_value.tv_nsec = 50*1000*1000UL;

    second_value.it_interval.tv_sec = 0; // 表示周期
    second_value.it_interval.tv_nsec = 50*1000*1000UL;

    if (timerfd_settime(fd_, 0, &second_value, NULL) == -1)
    {
        cout << LMSG << "timerfd_create err:" << strerror(errno) << endl;
    }
    else
    {
        EnableRead();
    }
}

TimerInMillSecond::~TimerInMillSecond()
{
}

int TimerInMillSecond::OnRead()
{
    RunEveryNMillSecond();

    uint64_t num_expired = 0;

    int bytes = read(fd_, &num_expired, sizeof(uint64_t));
    UNUSED(bytes);
    //cout << LMSG << "read " << bytes << " bytes, num_expired:" << num_expired << endl;

    return kSuccess;
}

int TimerInMillSecond::RunEveryNMillSecond()
{
    uint64_t now_ms = Util::GetNowMs();
    uint64_t time_elapse = now_ms - now_in_ms_;

    //cout << LMSG << "now_ms:" << now_ms << ",time elapse:" << time_elapse <<",count:" << count_ << endl;

    for (auto& handle : millsecond_handle_)
    {
        handle->HandleTimerInMillSecond(now_ms, time_elapse, count_);
    }

    ++count_;
    now_in_ms_ = now_ms;

    return kSuccess;
}
