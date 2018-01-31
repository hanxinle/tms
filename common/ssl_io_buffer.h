#ifndef __SSL_IO_BUFFER_H__
#define __SSL_IO_BUFFER_H__

#include <iostream>
#include <string>

#include "common_define.h"
#include "io_buffer.h"
#include "trace_tool.h"

#include "openssl/ssl.h"

using std::string;
using std::cout;
using std::endl;

class SslIoBuffer : public IoBuffer
{
public:
    SslIoBuffer(const size_t& capacity = 0);
    ~SslIoBuffer();

    void SetSsl(SSL* ssl)
    {
        ssl_ = ssl;
    }

    virtual int ReadFromFdAndWrite(const int& fd);
    virtual int WriteToFd(const int& fd);

private:
    SSL* ssl_;
};

#endif // __SSL_IO_BUFFER_H__
