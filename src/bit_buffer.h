#ifndef __BIT_BUFFER_H__
#define __BIT_BUFFER_H__

#include <iostream>
#include <string>

#include "common_define.h"

class BitBuffer
{
public:
    BitBuffer(const std::string& data);
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
        uint64_t tmp = 0;
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

    inline size_t BitsLeft()
    {
        if (bit_len_ >= cur_pos_)
        {
            return (bit_len_ - cur_pos_);
        }

        return 0;
    }

    inline size_t BytesLeft()
    {
        return BitsLeft()/8;
    }

    template<typename T>
    int GetBits(const size_t& bits, T& result)
    {
        if (! MoreThanBits(bits))
        {
            std::cout << LMSG << "no more than " << bits << " bits, left bits:" << BitsLeft() << std::endl;
            return -1;
        }

        result = 0;

        for (size_t i = 0; i != bits; ++i)
        {
            result <<= 1;
            
            // XXX: static mask
            uint8_t mask = (0x01 << (7 - cur_pos_%8));

            //std::cout << "cur_pos_:" << cur_pos_ << ",mask:" << (uint16_t)mask << std::endl;

            if (data_[cur_pos_/8] & mask)
            {
                result |= 1;
            }

            ++cur_pos_;
        }

        return 0;
    }
    int GetString(const size_t& len, std::string& result);

    int PeekBits(const size_t& bits, uint64_t& result);

    size_t HaveReadBytes()
    {
        return cur_pos_ / 8;
    }

    const uint8_t* CurData() const
    {
        return data_ + cur_pos_ / 8;
    }

    size_t CurLen() const
    {
        return (bit_len_ - cur_pos_) / 8;
    }

    void SkipBits(const size_t& bits)
    {
        size_t bit_left = BitsLeft();
        cur_pos_ += (bit_left <= bits ? bit_left : bits);
    }

    void SkipBytes(const size_t& bytes)
    {
        size_t byte_left = BytesLeft();
        cur_pos_ += (byte_left <= bytes ? byte_left : bytes) * 8;
    }

private:
    const uint8_t* data_;
    size_t bit_len_;
    size_t cur_pos_;
};

#endif // __BIT_BUFFER_H__
