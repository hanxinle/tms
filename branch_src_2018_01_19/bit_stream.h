#ifndef __BIT_STREAM_H__
#define __BIT_STREAM_H__

#include <string.h>

#include "common_define.h"
#include "trace_tool.h"

class BitStream
{
public:
    BitStream()
    {
        bzero(buf_, sizeof(buf_));
        bit_len_ = sizeof(buf_)*8;
        cur_pos_ = 0;
    }

    template<typename T>
    int WriteBits(const size_t& bits, const T& val)
    {
#if 1
        if (cur_pos_ + bits > bit_len_)
        {
            cout << LMSG << "write " << bits << " bits will be overflow" << endl;
            return -1;
        }

        T mask = 1UL << (bits - 1);

        for (size_t i = 0; i != bits; ++i)
        {
            if (val & mask)
            {
                buf_[cur_pos_/8] |= (1 << (7-(cur_pos_%8)));
            }

            mask >>= 1;
            ++cur_pos_;
        }
#else
        T mask = 1UL<<(sizeof(T)*8-1);
        T tmp = val << (sizeof(T)*8-bits);

        for (size_t b = 0; b != bits; ++b)
        {
            uint32_t index = cur_pos_/8;
            if (index*8 > bit_len_)
            {
                cout << LMSG << "write bits overflow" << endl;
                return -1;
            }

            if (tmp & mask)
            {
                buf_[cur_pos_/8] |= (1 << (7-(cur_pos_%8)));
            }
            tmp <<= 1;
            ++cur_pos_;
        }
#endif

        return 0;
    }

    template<typename T>
    int WriteBytes(const size_t& bytes, const T& val)
    {
#if 1
        const uint8_t* p = (const uint8_t*)&val;

        for (size_t i = 0; i != bytes; ++i)
        {
            buf_[cur_pos_/8 + i] = p[bytes-1-i];
        }

        cur_pos_ += bytes * 8;

        return 0;
#else
        return WriteBits<T>(bytes*8, val);
#endif
    }

    int WriteData(const size_t& bytes, const uint8_t* data)
    {
        memcpy(buf_ + cur_pos_/8, data, bytes);
        cur_pos_ += bytes * 8;

        return 0;
    }

    uint32_t SizeInBytes()
    {
        return cur_pos_ / 8;
    }

    uint8_t* GetData()
    {
        return buf_;
    }

private:
    uint8_t buf_[4096];
    uint32_t bit_len_;
    uint32_t cur_pos_;
};

#endif // __BIT_STREAM_H__
