#ifndef __SERIALIZE_H__
#define __SERIALIZE_H__

#include <stdint.h>

#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "rpc.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace protocol
{

class Serialize
{
public:
    Serialize()
    {
        capacity_ = 1024;
        size_ = kHeaderSize;
        buf_ = (uint8_t*)malloc(capacity_);
    }

    ~Serialize()
    {
        if (buf_ != NULL)
        {
            free(buf_);
        }
    }

    const uint8_t* GetBuf() const
    {
        return buf_;
    }

    const uint32_t GetSize() const
    {
        return size_;
    }

    const uint32_t GetCapacity() const
    {
        return capacity_;
    }

    void Write(const uint8_t& u8)
    {
        MakeCapacityIfNeed(sizeof(u8));

        buf_[size_++] = u8;
    }

    void Write(const uint16_t& u16)
    {
        MakeCapacityIfNeed(sizeof(u16));

        const uint8_t* p = (const uint8_t*)&u16;

        buf_[size_++] = p[0];
        buf_[size_++] = p[1];
    }

    void Write(const uint32_t& u32)
    {
        MakeCapacityIfNeed(sizeof(u32));

        const uint8_t* p = (const uint8_t*)&u32;

        buf_[size_++] = p[0];
        buf_[size_++] = p[1];
        buf_[size_++] = p[2];
        buf_[size_++] = p[3];
    }

    void Write(const uint64_t& u64)
    {
        MakeCapacityIfNeed(sizeof(u64));

        const uint8_t* p = (const uint8_t*)&u64;

        buf_[size_++] = p[0];
        buf_[size_++] = p[1];
        buf_[size_++] = p[2];
        buf_[size_++] = p[3];
        buf_[size_++] = p[4];
        buf_[size_++] = p[5];
        buf_[size_++] = p[6];
        buf_[size_++] = p[7];
    }

    void Write(const int8_t& s8)
    {
        MakeCapacityIfNeed(sizeof(s8));

        buf_[size_++] = s8;
    }

    void Write(const int16_t& s16)
    {
        MakeCapacityIfNeed(sizeof(s16));

        const uint8_t* p = (const uint8_t*)&s16;

        buf_[size_++] = p[0];
        buf_[size_++] = p[1];
    }

    void Write(const int32_t& s32)
    {
        MakeCapacityIfNeed(sizeof(s32));

        const uint8_t* p = (const uint8_t*)&s32;

        buf_[size_++] = p[0];
        buf_[size_++] = p[1];
        buf_[size_++] = p[2];
        buf_[size_++] = p[3];
    }

    void Write(const int64_t& s64)
    {
        MakeCapacityIfNeed(sizeof(s64));

        const uint8_t* p = (const uint8_t*)&s64;

        buf_[size_++] = p[0];
        buf_[size_++] = p[1];
        buf_[size_++] = p[2];
        buf_[size_++] = p[3];
        buf_[size_++] = p[4];
        buf_[size_++] = p[5];
        buf_[size_++] = p[6];
        buf_[size_++] = p[7];
    }

    void Write(const string& str32)
    {
        MakeCapacityIfNeed(sizeof(uint32_t) + str32.size());

        Write((uint32_t)str32.size());

        memcpy(buf_ + size_, str32.data(), str32.size());

        size_ += str32.size();
    }

    template<typename T>
    void Write(const vector<T>& vec)
    {
        MakeCapacityIfNeed(sizeof(uint32_t));

        Write((uint32_t)vec.size());

        for (const auto& v : vec)
        {
            Write(v);
        }
    }

    template<typename K, typename V>
    void Write(const map<K, V>& m)
    {
        MakeCapacityIfNeed(sizeof(uint32_t));

        Write((uint32_t)m.size());

        for (const auto& kv : m)
        {
            Write(kv.first);
            Write(kv.second);
        }
    }

    template<typename V>
    void Write(const set<V>& s)
    {
        MakeCapacityIfNeed(sizeof(uint32_t));

        Write((uint32_t)s.size());

        for (const auto& v : s)
        {
            Write(v);
        }
    }

    void WriteHeader(const uint32_t& protocol_id)
    {
        MakeCapacityIfNeed(kHeaderSize);

        const uint8_t* p = (const uint8_t*)&size_;

        buf_[0] = p[0];
        buf_[1] = p[1];
        buf_[2] = p[2];
        buf_[3] = p[3];

        p = (const uint8_t*)&protocol_id;

        buf_[4] = p[0];
        buf_[5] = p[1];
        buf_[6] = p[2];
        buf_[7] = p[3];
    }

private:
    void MakeCapacityIfNeed(const uint64_t& delta)
    {
        if (size_ + delta > capacity_)
        {
            int new_capacity = size_ + delta;

            if (capacity_ * 2 > new_capacity)
            {
                new_capacity = capacity_ * 2;
            }

            if (capacity_ + 1024 > new_capacity)
            {
                new_capacity = capacity_ + 1024;
            }

            uint8_t* new_buf = (uint8_t*)malloc(new_capacity);

            memcpy(new_buf, buf_, size_);

            free(buf_);

            buf_ = new_buf;
            buf_ = buf_ + size_;
        }
    }

private:
    uint32_t capacity_;
    uint32_t size_;
    uint8_t* buf_;
};

} // namespace protocol

#endif // __SERIALIZE_H__
