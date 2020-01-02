#include <unistd.h>
#include <string.h>

#include <iostream>

#include "common_define.h"
#include "ssl_io_buffer.h"
#include "util.h"

#include "openssl/err.h"

using namespace std;

SslIoBuffer::SslIoBuffer(const size_t& capacity)
    :
    IoBuffer(capacity)
{
}

SslIoBuffer::~SslIoBuffer()
{
}

int SslIoBuffer::ReadFromFdAndWrite(const int& fd)
{
    UNUSED(fd);

    MakeSpaceIfNeed(kEnlargeSize);

    //VERBOSE << LMSG << "SslIoBuffer capacity:" << CapacityLeft() << endl;
    
    int max_read = CapacityLeft();
    if (max_read >= 1024*8)
    {
        max_read = 1024*8;
    }

    int bytes = SSL_read(ssl_, end_, max_read);

    if (bytes > 0)
    {
        //VERBOSE << LMSG << "read " << bytes << " bytes" << endl;
        end_ += bytes;
    }
    else if (bytes == 0)
    {
        cout << LMSG << "close by peer" << endl;
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
    cout << LMSG << "size:" << max_write << endl;

    if (max_write >= 1024*8)
    {
        max_write = 1024*8;
    }

    int ret = SSL_write(ssl_, start_, max_write);

    char buf[1024] = {0};
    char* err = ERR_error_string(ERR_get_error(), buf);
    cout << LMSG << "err:" << err << endl;

    if (ret > 0)
    {
        start_ += ret;
    }
    else if (ret == 0)
    {
    }
    else
    {
        int err = SSL_get_error(ssl_, ret);
        cout << LMSG << "ssl_:" << ssl_ << ",peek:" << Util::Bin2Hex(start_, 10) << endl;
        cout << LMSG << "start:" << (long)start_ << ",max_write:" << max_write << ",ret:" << ret << ",err:" << err << "," << strerror(errno) << endl;
    }

    return ret;
}
