#ifndef __IO_BUFFER_H__
#define __IO_BUFFER_H__

#include <iostream>
#include <string>

#include "common_define.h"

using std::string;
using std::cout;
using std::endl;

class IoBuffer
{
public:
    IoBuffer(const size_t& capacity = 0);
    ~IoBuffer();

    int ReadFromFdAndWrite(const int& fd);
    int WriteToFd(const int& fd);

    int Write(const string& data);
    int Write(const uint8_t* data, const size_t& len);
    int WriteU8(const uint8_t& u8);
    int WriteU16(const uint16_t& u16);
    int WriteU24(const uint32_t& u24);
    int WriteU32(const uint32_t& u32);
    int WriteU64(const uint64_t& u64);
    int WriteFake(const size_t& len);

    int ReadAndCopy(uint8_t* data, const size_t& len);
    int Read(uint8_t*& data, const size_t& len);
    int ReadU8(uint8_t& u8);
    int ReadU16(uint16_t& u16);
    int ReadU32(uint32_t& u32);
    int ReadU64(uint64_t& u64);

    int Peek(uint8_t*& data, const size_t& begin_pos, const size_t& len);
    int Skip(const size_t& len);

    bool Empty()
    {
        return Size() == 0;
    }

    int Size()
    {
        if (buf_ == NULL)
        {
            return 0;
        }

        if (end_ == start_)
        {
            start_ = buf_;
            end_ = buf_;

#ifdef DEBUG
            cout << LMSG << "adjust start_ and end_ to buf_" << endl;
#endif
        }

        return end_ - start_;
    }

    int CapacityLeft()
    {
        return capacity_ - (end_ - buf_);
    }

private:
    int MakeSpaceIfNeed(const size_t& len);

private:
    uint8_t *buf_;
    uint64_t capacity_;

    // buf [----(start)----------(end)----]
    //     |---->      capacity      <----|
    uint8_t *start_;
    uint8_t *end_;
};

#endif // __IO_BUFFER_H__
