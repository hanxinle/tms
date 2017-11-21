#ifndef __BIT_BUFFER_H__
#define __BIT_BUFFER_H__

#include <iostream>
#include <string>

#include "common_define.h"

using std::string;
using std::cout;
using std::endl;

class BitBuffer
{
public:
    BitBuffer(const string& data);
    BitBuffer(const uint8_t* data, const size_t& len);

    inline bool MoreThanBytes(const size_t& bytes)
    {
        return MoreThanBits(bytes*8);
    }

    inline bool MoreThanBits(const size_t& bits)
    {
        return BitsLeft() >= bits;
    }

    template<typename T>
    int GetBytes(const size_t& bytes, T& result)
    {
        uint64_t tmp;
        int ret = GetBits(bytes*8, tmp);

        result = (T)tmp;

        return ret;
    }

    template<typename T>
    int PeekBytes(const size_t& bytes, T& result)
    {
        uint64_t tmp;
        int ret = PeekBits(bytes*8, tmp);

        result = (T)tmp;

        return ret;
    }

    inline int BitsLeft()
    {
        if (bit_len_ >= cur_pos_)
        {
            return (bit_len_ - cur_pos_);
        }

        return 0;
    }

    inline int BytesLeft()
    {
        return BitsLeft()/8;
    }

    template<typename T>
    int GetBits(const size_t& bits, T& result)
    {
        if (! MoreThanBits(bits))
        {
            cout << LMSG << "no more than " << bits << " bits, left bits:" << BitsLeft() << endl;
            return -1;
        }

        result = 0;

        for (size_t i = 0; i != bits; ++i)
        {
            result <<= 1;
            
            // XXX: static mask
            uint8_t mask = (0x01 << (7 - cur_pos_%8));

            //cout << "cur_pos_:" << cur_pos_ << ",mask:" << (uint16_t)mask << endl;

            if (data_[cur_pos_/8] & mask)
            {
                result |= 1;
            }

            ++cur_pos_;
        }

        return 0;
    }
    int GetString(const size_t& len, string& result);

    int PeekBits(const size_t& bits, uint64_t& result);

private:
    const uint8_t* data_;
    size_t bit_len_;
    size_t cur_pos_;
};

#endif // __BIT_BUFFER_H__
