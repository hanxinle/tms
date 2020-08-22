#if defined(__APPLE__)

#include "timer_in_second.h"

#include <string.h>

#include "common_define.h"
#include "timer_handle.h"
#include "util.h"

TimerInSecond::TimerInSecond(IoLoop* io_loop) : Fd(io_loop) {}
TimerInSecond::~TimerInSecond() {}

int TimerInSecond::OnRead() { return kSuccess; }

int TimerInSecond::RunEverySecond() { return kSuccess; }
#else
#include <string.h>
#include <sys/timerfd.h>

#include "common_define.h"
#include "timer_handle.h"
#include "timer_in_second.h"
#include "util.h"

TimerInSecond::TimerInSecond(IoLoop* io_loop)
    : Fd(io_loop, timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
      now_in_ms_(Util::GetNowMs()),
      count_(1) {
  if (fd_ == -1) {
    std::cout << LMSG << "timerfd_create err:" << strerror(errno) << std::endl;
  }

  itimerspec second_value;

  second_value.it_value.tv_sec = 1;  // 表示过多少秒之后开始
  second_value.it_value.tv_nsec = 0;

  second_value.it_interval.tv_sec = 1;  // 表示周期
  second_value.it_interval.tv_nsec = 0;

  if (timerfd_settime(fd_, 0, &second_value, NULL) == -1) {
    std::cout << LMSG << "timerfd_create err:" << strerror(errno) << std::endl;
  } else {
    EnableRead();
  }
}

TimerInSecond::~TimerInSecond() {}

int TimerInSecond::OnRead() {
  RunEverySecond();

  uint64_t num_expired = 0;

  int bytes = read(fd_, &num_expired, sizeof(uint64_t));
  UNUSED(bytes);
  // std::cout << LMSG << "read " << bytes << " bytes, num_expired:" <<
  // num_expired << std::endl;

  return kSuccess;
}

int TimerInSecond::RunEverySecond() {
  uint64_t now_ms = Util::GetNowMs();
  uint64_t time_elapse = now_ms - now_in_ms_;

  // std::cout << LMSG << "now_ms:" << now_ms << ",time elapse:" << time_elapse
  // <<",count:" << count_ << std::endl;

  for (auto& handle : second_handle_) {
    handle->HandleTimerInSecond(now_ms, time_elapse, count_);
  }

  ++count_;
  now_in_ms_ = now_ms;

  return kSuccess;
}
#endif
