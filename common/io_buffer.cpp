#include <unistd.h>
#include <string.h>

#include <iostream>

#include "common_define.h"
#include "io_buffer.h"
#include "util.h"

IoBuffer::IoBuffer(const size_t& capacity)
    : capacity_(capacity)
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
        //VERBOSE << LMSG << "capacity_:" << capacity_ << ",Size():" << Size() << ",CapacityLeft():" << CapacityLeft() << ",buf:" << (void*)buf_ << std::endl;

        free(buf_);
        buf_ = NULL;

        capacity_ = 0;
        
        start_ = NULL;
        end_   = NULL;
    }
}

int IoBuffer::ReadFromFdAndWrite(const int& fd)
{
    MakeSpaceIfNeed(kEnlargeSize);

    //VERBOSE << LMSG << "IoBuffer capacity:" << CapacityLeft() << std::endl;
    size_t max_read = CapacityLeft();
    if (max_read > 1024*64)
    {
        max_read = 1024*64;
    }

    int bytes = read(fd, end_, max_read);

    if (bytes > 0)
    {
        //VERBOSE << LMSG << "read " << bytes << " bytes" << std::endl;
        end_ += bytes;
    }
    else if (bytes == 0)
    {
        std::cout << LMSG << "close by peer" << std::endl;
    }
    else
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            std::cout << LMSG << "read err:" << strerror(errno) << std::endl;
        }
    }

    return bytes;
}

int IoBuffer::ReadFromFdAndWrite(const int& fd, sockaddr* addr, socklen_t* addr_len)
{
    MakeSpaceIfNeed(kUdpMaxSize);

    //VERBOSE << LMSG << "IoBuffer capacity:" << CapacityLeft() << std::endl;

    int bytes = recvfrom(fd, end_, CapacityLeft(), 0, addr, addr_len);

    if (bytes > 0)
    {
        //VERBOSE << LMSG << "read " << bytes << " bytes" << std::endl;
        end_ += bytes;
    }
    else if (bytes == 0)
    {
        std::cout << LMSG << "udp read 0 bytes is impossible" << std::endl;
        assert(false);
    }
    else
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            std::cout << LMSG << "read err:" << strerror(errno) << std::endl;
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

    if (ret > 0)
    {
        start_ += ret;
    }

    return ret;
}

int IoBuffer::Write(const std::string& data)
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
    assert(end_ >= start_);

    size_t cur_capacity = CapacityLeft();
    size_t start_pos = start_ - buf_;
    size_t end_pos = end_ - buf_;

    if (cur_capacity >= len)
    {
        return 0;
    }

    size_t new_capacity = std::max(capacity_ + len, capacity_ * 2);

    //std::cout << LMSG << "cur_capacity:" << cur_capacity << ",new_capacity:" << new_capacity << std::endl;

    buf_ = (uint8_t*)realloc(buf_, new_capacity);

    if (buf_ == NULL)
    {
        return -1;
    }

    start_ = buf_ + start_pos;
    end_ = buf_ + end_pos;

    assert(end_ >= start_);

    capacity_ = new_capacity;

    return 0;
}

int IoBuffer::ReadAndCopy(uint8_t* data, const size_t& len)
{
    size_t size = Size();

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
    size_t size = Size();

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
    if (Size() < (begin_pos + len))
    {
        std::cout << LMSG << "[" << begin_pos << "," << (begin_pos + len) << ") overflow" << std::endl;
        return -1;
    }

    data = start_ + begin_pos;

    return len;
}

int IoBuffer::PeekU8(uint8_t& u8)
{
    if (Size() < sizeof(uint8_t))
    {
        return -1;
    }

    const uint8_t* tmp = start_;

    u8 = 0;
    u8 = *tmp++;

    return sizeof(uint8_t);
}

int IoBuffer::PeekU16(uint16_t& u16)
{
    if (Size() < sizeof(uint16_t))
    {
        return -1;
    }

    const uint8_t* tmp = start_;

    u16 = 0;
    u16 |= *tmp++;
    u16 <<= 8;
    u16 |= *tmp++;

    return sizeof(uint16_t);
}

int IoBuffer::PeekU32(uint32_t& u32)
{
    if (Size() < sizeof(uint32_t))
    {
        return -1;
    }

    const uint8_t* tmp = start_;

    u32 = 0;
    u32 |= *tmp++;
    u32 <<= 8;
    u32 |= *tmp++;
    u32 <<= 8;
    u32 |= *tmp++;
    u32 <<= 8;
    u32 |= *tmp++;

    return sizeof(uint32_t);
}

int IoBuffer::PeekU64(uint64_t& u64)
{
    if (Size() < sizeof(uint64_t))
    {
        return -1;
    }

    const uint8_t* tmp = start_;

    u64 = 0;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;
    u64 <<= 8;
    u64 |= *tmp++;

    return sizeof(uint64_t);
}

int IoBuffer::Skip(const size_t& len)
{
    if (Size() < len)
    {
        std::cout << LMSG << "len:" << len << " overflow" << std::endl;
    }

    start_ += len;

    return len;
}
