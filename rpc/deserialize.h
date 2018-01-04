#ifndef __DESERIALIZE_H__
#define __DESERIALIZE_H__

#include <stdint.h>

#include <string.h>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "rpc.h"

using std::cout;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace protocol
{

class Deserialize
{
public:
    Deserialize(const uint8_t* data, const uint32_t& len)
    {
        capacity_ = len;
        size_ = kHeaderSize;
        buf_ = data;
    }

    ~Deserialize()
    {
    }

    int Read(uint8_t& u8)
    {
        if (size_ + sizeof(u8) > capacity_)
        {
            return -1;
        }

        u8 = buf_[size_++];

        return 0;
    }

    int Read(uint16_t& u16)
    {
        if (size_ + sizeof(u16) > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&u16;

        *p++ = buf_[size_++];
        *p++ = buf_[size_++];

        return 0;
    }

    int Read(uint32_t& u32)
    {
        if (size_ + sizeof(u32) > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&u32;

        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];

        return 0;
    }

    int Read(uint64_t& u64)
    {
        if (size_ + sizeof(u64) > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&u64;

        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];

        return 0;
    }

    int Read(int8_t& s8)
    {
        if (size_ + sizeof(s8) > capacity_)
        {
            return -1;
        }

        s8 = buf_[size_++];

        return 0;
    }

    int Read(int16_t& s16)
    {
        if (size_ + sizeof(s16) > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&s16;

        *p++ = buf_[size_++];
        *p++ = buf_[size_++];

        return 0;
    }

    int Read(int32_t& s32)
    {
        if (size_ + sizeof(s32) > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&s32;

        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];

        return 0;
    }

    int Read(int64_t& s64)
    {
        if (size_ + sizeof(s64) > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&s64;

        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];
        *p++ = buf_[size_++];

        return 0;
    }

    int Read(string& str32)
    {
        uint32_t str_len = 0;
        if (Read(str_len) < 0 || size_ + str_len > capacity_)
        {
            return -1;
        }

        str32.assign((char*)buf_ + size_, str_len);

        size_ += str_len;

        return 0;
    }

    template<typename T>
    int Read(vector<T>& vec)
    {
        uint32_t vec_size = 0;
        if (Read(vec_size) < 0 || size_ + vec_size > capacity_)
        {
            return -1;
        }

        for (uint32_t i = 0; i != vec_size; ++i)
        {
            typename vector<T>::value_type v;

            if (Read(v) < 0)
            {
                return -1;
            }

            vec.push_back(v);
        }

        return 0;
    }

    template<typename K, typename V>
    int Read(map<K, V>& m)
    {
        uint32_t map_size = 0;
        if (Read(map_size) < 0 || size_ + map_size > capacity_)
        {
            return -1;
        }

        for (uint32_t i = 0; i != map_size; ++i)
        {
            K k;
            V v;

            if (Read(k) < 0)
            {
                return -1;
            }

            if (Read(v) < 0)
            {
                return -1;
            }

            m.insert(make_pair(k, v));
        }

        return 0;
    }

    template<typename T>
    int Read(set<T>& s)
    {
        uint32_t set_size = 0;
        if (Read(set_size) < 0 || size_ + set_size > capacity_)
        {
            return -1;
        }

        for (uint32_t i = 0; i != set_size; ++i)
        {
            typename set<T>::value_type v;

            if (Read(v) < 0)
            {
                return -1;
            }

            s.insert(v);
        }

        return 0;
    }

    int ReadLen(uint32_t& len)
    {
        if (kHeaderSize > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&len;

        p[0] = buf_[0];
        p[1] = buf_[1];
        p[2] = buf_[2];
        p[3] = buf_[3];

        return 0;
    }

    int ReadProtocolId(uint32_t& protocol_id)
    {
        if (kHeaderSize > capacity_)
        {
            return -1;
        }

        uint8_t* p = (uint8_t*)&protocol_id;

        p[0] = buf_[4];
        p[1] = buf_[5];
        p[2] = buf_[6];
        p[3] = buf_[7];

        return 0;
    }

    uint32_t GetSize()
    {
        return size_;
    }

private:
    uint32_t capacity_;
    uint32_t size_;
    const uint8_t* buf_;
};

} // namespace protocol

#endif // __DESERIALIZE_H__
