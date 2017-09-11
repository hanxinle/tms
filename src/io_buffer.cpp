#include <iostream>
#include <string.h>

#include "common_define.h"
#include "io_buffer.h"
#include "util.h"

using namespace std;

IoBuffer::IoBuffer(const size_t& capacity)
    :
    capacity_(capacity)
{
    if (capacity_ != 0)
    {
        buf_ = (uint8_t*)malloc(capacity_);
        start_ = buf_;
        end_ = buf_;
    }
    else
    {
        buf_ = NULL;
        start_ = NULL;
        end_ = NULL;
    }
}

IoBuffer::~IoBuffer()
{
    if (buf_ != NULL)
    {
        cout << LMSG << "capacity_:" << capacity_ << ",Size():" << Size() << ",CapacityLeft():" << CapacityLeft() << ",buf:" << (void*)buf_ << endl;
        free(buf_);
        buf_ = NULL;

        capacity_ = 0;
        
        start_ = NULL;
        end_   = NULL;
    }
}

int IoBuffer::ReadFromFdAndWrite(const int& fd)
{
    MakeSpaceIfNeed(1024*64);

    cout << LMSG << "IoBuffer capacity:" << CapacityLeft() << endl;

    int bytes = read(fd, end_, CapacityLeft());

    if (bytes > 0)
    {
        cout << LMSG << "read " << bytes << " bytes" << endl;
        end_ += bytes;
    }
    else if (bytes == 0)
    {
        cout << LMSG << "close by peer" << endl;
    }
    else
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            cout << LMSG << "read err:" << strerror(errno) << endl;
        }
    }

    return bytes;
}

int IoBuffer::WriteToFd(const int& fd)
{
    if (Empty())
    {
        return 0;
    }

    int ret = write(fd, start_, Size());

    cout << LMSG << "write " << ret << " bytes" << endl;

    if (ret > 0)
    {
        start_ += ret;
    }

    return ret;
}

int IoBuffer::Write(const string& data)
{
    return Write((const uint8_t*)data.data(), data.length());
}

int IoBuffer::Write(const uint8_t* data, const size_t& len)
{
    int ret = MakeSpaceIfNeed(len);
    if (ret < 0)
    {
        return ret;
    }

    memcpy(end_, data, len);
    end_ += len;

    return len;
}

int IoBuffer::WriteU8(const uint8_t& u8)
{
    const uint8_t* data = &u8;
    size_t len = sizeof(uint8_t);

    return Write(data, len);
}

int IoBuffer::WriteU16(const uint16_t& u16)
{
    uint16_t be16 = htobe16(u16);

    const uint8_t* data = (const uint8_t*)&be16;
    size_t len = sizeof(uint16_t);

    return Write(data, len);
}

int IoBuffer::WriteU24(const uint32_t& u24)
{
    uint32_t be24 = htobe32(u24);

    const uint8_t* data = (const uint8_t*)&be24;
    size_t len = 3;

    return Write(data + 1, len);
}

int IoBuffer::WriteU32(const uint32_t& u32)
{
    uint32_t be32 = htobe32(u32);

    const uint8_t* data = (const uint8_t*)&be32;
    size_t len = sizeof(uint32_t);

    return Write(data, len);
}

int IoBuffer::WriteU64(const uint64_t& u64)
{
    uint64_t be64 = htobe64(u64);

    const uint8_t* data = (const uint8_t*)&be64;
    size_t len = sizeof(uint64_t);

    return Write(data, len);
}

int IoBuffer::WriteFake(const size_t& len)
{
    int ret = MakeSpaceIfNeed(len);
    if (ret < 0)
    {
        return ret;
    }

    end_ += len;

    return ret;
}

int IoBuffer::MakeSpaceIfNeed(const size_t& len)
{
    size_t cur_capacity = CapacityLeft();
    size_t start_pos = start_ - buf_;
    size_t end_pos = end_ - buf_;

    if (cur_capacity >= len)
    {
        return 0;
    }

    size_t new_capacity = max(capacity_ + len, capacity_ * 2);

    cout << LMSG << "cur_capacity:" << cur_capacity << ",new_capacity:" << new_capacity << endl;

    buf_ = (uint8_t*)realloc(buf_, new_capacity);

    if (buf_ == NULL)
    {
        return -1;
    }

    start_ = buf_ + start_pos;
    end_ = buf_ + end_pos;

    capacity_ = new_capacity;

    return 0;
}

int IoBuffer::ReadAndCopy(uint8_t* data, const size_t& len)
{
    int size = Size();

    if (size == 0)
    {
        return 0;
    }

    if (len >= size)
    {
        memcpy(data, start_, size);
        start_ += size;
        return size;
    }

    memcpy(data, start_, len);
    start_ += len;

    return len;
}

int IoBuffer::Read(uint8_t*& data, const size_t& len)
{
    int size = Size();

    if (size == 0)
    {
        return 0;
    }

    if (len >= size)
    {
        data = start_;
        start_ += size;
        return size;
    }

    data = start_;
    start_ += len;

    return len;
}

int IoBuffer::ReadU8(uint8_t& u8)
{
    if (Size() < sizeof(uint8_t))
    {
        return -1;
    }

    u8 = 0;
    u8 = *start_++;

    return sizeof(uint8_t);
}

int IoBuffer::ReadU16(uint16_t& u16)
{
    if (Size() < sizeof(uint16_t))
    {
        return -1;
    }

    u16 = 0;
    u16 |= *start_++;
    u16 <<= 8;
    u16 |= *start_++;

    return sizeof(uint16_t);
}

int IoBuffer::ReadU32(uint32_t& u32)
{
    if (Size() < sizeof(uint32_t))
    {
        return -1;
    }

    u32 = 0;
    u32 |= *start_++;
    u32 <<= 8;
    u32 |= *start_++;
    u32 <<= 8;
    u32 |= *start_++;
    u32 <<= 8;
    u32 |= *start_++;

    return sizeof(uint32_t);
}

int IoBuffer::ReadU64(uint64_t& u64)
{
    if (Size() < sizeof(uint64_t))
    {
        return -1;
    }

    u64 = 0;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;
    u64 <<= 8;
    u64 |= *start_++;

    return sizeof(uint64_t);
}

int IoBuffer::Peek(uint8_t*& data, const size_t& begin_pos, const size_t& len)
{
    if (Size() < begin_pos + len)
    {
        cout << LMSG << "[" << begin_pos << "," << (begin_pos + len) << ") overflow" << endl;
        return -1;
    }

    data = start_ + begin_pos;

    return len;
}

int IoBuffer::Skip(const size_t& len)
{
    if (Size() < len)
    {
        cout << LMSG << "len:" << len << " overflow" << endl;
    }

    start_ += len;

    return len;
}
