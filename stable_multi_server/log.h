#ifndef __LOG_H__
#define __LOG_H__

#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::ostringstream;
using std::ostream;

class Log
{
public:
    Log(const uint8_t& log_level) 
        :
        log_level_(log_level)
    { 
    }

    ~Log() 
    { 
        if (log_level_ >= g_log_level_)
        {
            cout << os_.str();
        }
    }

    template<typename T>
    Log& operator<<(const T& t)
    {
        if (log_level_ >= g_log_level_)
        {
            os_ << t;
        }
        return *this;
    }

    Log& operator<<(ostream& (*pf)(ostream&))
    {
        if (log_level_ >= g_log_level_)
        {
            os_ << pf;
        }
        return *this;
    }

public:
    static uint8_t g_log_level_;
    static void SetLogLevel(const uint8_t level)
    {
        g_log_level_ = level;
    }

private:
    ostringstream os_;
    uint8_t log_level_;
};

#endif // __LOG_H__
