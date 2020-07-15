#ifndef __EPOLLER_H__
#define __EPOLLER_H__

#include "io_loop.h"

class Fd;

class Epoller : public IoLoop {
 public:
  Epoller();
  ~Epoller();

  int Create();
  void RunIOLoop(const int& timeout_in_millsecond);

  int AddFd(Fd* fd);
  int DelFd(Fd* fd);
  int ModFd(Fd* fd);

  void WaitIO(const int& timeout_in_millsecond);
};

#endif  // __EPOLLER_H__
