#ifndef __SRT_EPOLLER_H__
#define __SRT_EPOLLER_H__

#include "io_loop.h"

#include <map>

class Fd;

class SrtEpoller : public IoLoop
{
public:
    SrtEpoller();
    ~SrtEpoller();

    int Create();
    void RunIOLoop(const int& timeout_in_millsecond);

    int AddFd(Fd* fd);
    int DelFd(Fd* fd);
    int ModFd(Fd* fd);

    void WaitIO(const int& timeout_in_millsecond);

private:
    std::map<int, Fd*> srt_socket_map_;
};
    
#endif // __SRT_EPOLLER_H__
