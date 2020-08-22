#ifndef __TIMER_IN_MILLSECOND_H__
#define __TIMER_IN_MILLSECOND_H__

#include <set>

#include "common_define.h"
#include "fd.h"

class TimerMillSecondHandle;

class TimerInMillSecond : public Fd {
 public:
  TimerInMillSecond(IoLoop* io_loop);
  ~TimerInMillSecond();

  int RunEveryNMillSecond();

  bool AddTimerMillSecondHandle(TimerMillSecondHandle* handle) {
    auto iter = millsecond_handle_.insert(handle);

    return iter.second;
  }

  int Send(const uint8_t* data, const size_t& len) {
    UNUSED(data);
    UNUSED(len);

    return 0;
  }

  int OnRead();
  int OnWrite() { return 0; }

 private:
  std::set<TimerMillSecondHandle*> millsecond_handle_;

  uint64_t now_in_ms_;
  uint64_t count_;
};

#endif  // __TIMER_IN_MILLSECOND_H__
