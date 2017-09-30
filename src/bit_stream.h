#ifndef __BIT_STREAM_H__
#define __BIT_STREAM_H__

#include <string.h>

#include "common_define.h"

class BitStream
{
public:
    BitStream(const size_t& size_in_bytes = 1024)
    {
        buf_ = (uint8_t*)malloc(size_in_bytes);
        bzero(buf_, size_in_bytes);
        bit_len_ = size_in_bytes*8;
        cur_pos_ = 0;
    }

    ~BitStream()
    {
        if (buf_ != NULL)
        {
            free(buf_);
        }
    }

    template<typename T>
    int WriteBits(const size_t& bits, const T& val)
    {
#ifdef DEBUG
        cout << LMSG << (sizeof(T)*8-1) << endl;
#endif
        T mask = 1UL<<(sizeof(T)*8-1);
        T tmp = val << (sizeof(T)*8-bits);

#ifdef DEBUG
        cout << LMSG << "tmp:" << (uint64_t)tmp << ",val:" << (uint64_t)val << endl;
#endif

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

        return 0;
    }

    template<typename T>
    int WriteBytes(const size_t& bytes, const T& val)
    {
        return WriteBits<T>(bytes*8, val);
    }

    int WriteData(const size_t& bytes, const uint8_t* data)
    {
        for (size_t i = 0; i != bytes; ++i)
        {
            if (WriteBytes(1, data[i]) != 0)
            {
                return -1;
            }
        }

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
    uint8_t* buf_;
    uint32_t bit_len_;
    uint32_t cur_pos_;
};

#endif // __BIT_STREAM_H__
