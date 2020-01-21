#ifndef __IO_LOOP_H__
#define __IO_LOOP_H__

#include "io_loop.h"

class Fd;

class IoLoop
{
public:
    IoLoop() 
        : poll_fd_(-1)
        , quit_(false)
    {
    }

    virtual ~IoLoop() {}

    virtual int Create() = 0;
    virtual void RunIOLoop(const int& timeout_in_millsecond) = 0;

    virtual int AddFd(Fd* fd) = 0;
    virtual int DelFd(Fd* fd) = 0;
    virtual int ModFd(Fd* fd) = 0;

    virtual void WaitIO(const int& timeout_in_millsecond) = 0;

protected:
    int         poll_fd_;
    bool        quit_;
};
    
#endif // __IO_LOOP_H__
