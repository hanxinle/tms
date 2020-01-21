#include <unistd.h>
#include <string.h>

#include <iostream>

#include "common_define.h"
#include "ssl_io_buffer.h"
#include "util.h"

#include "openssl/err.h"

SslIoBuffer::SslIoBuffer(const size_t& capacity)
    : IoBuffer(capacity)
{
}

SslIoBuffer::~SslIoBuffer()
{
}

int SslIoBuffer::ReadFromFdAndWrite(const int& fd)
{
    UNUSED(fd);

    MakeSpaceIfNeed(kEnlargeSize);

    int max_read = CapacityLeft();
    if (max_read >= 1024*8)
    {
        max_read = 1024*8;
    }

    int bytes = SSL_read(ssl_, end_, max_read);

    if (bytes > 0)
    {
        end_ += bytes;
    }
    else if (bytes == 0)
    {
        std::cout << LMSG << "close by peer" << std::endl;
    }
    else
    {
    }

    return bytes;
}

int SslIoBuffer::WriteToFd(const int& fd)
{
    UNUSED(fd);

    if (Empty())
    {
        return 0;
    }

    size_t max_write = Size();

    if (max_write >= 1024*8)
    {
        max_write = 1024*8;
    }

    int ret = SSL_write(ssl_, start_, max_write);

    char buf[1024] = {0};
    char* err = ERR_error_string(ERR_get_error(), buf);

    if (ret > 0)
    {
        start_ += ret;
    }
    else
    {
        std::cout << LMSG << "ssl write failed" << std::endl;
    }

    return ret;
}
