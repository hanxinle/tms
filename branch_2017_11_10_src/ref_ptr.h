#ifndef __REF_PTR_H__
#define __REF_PTR_H__

#include <assert.h>

#include <atomic>
#include <iostream>

#include "common_define.h"

using std::atomic;
using std::cout;
using std::endl;
using std::hex;

class RefPtr
{
public:
    RefPtr(uint8_t* ptr)
        :
        ptr_(ptr),
        ref_count_(1)
    {
    }

    ~RefPtr()
    {
        assert(ref_count_ == 0);

        if(ptr_ != NULL)
        {
            free(ptr_);
        }
    }

    uint32_t AddRefCount()
    {
        ++ref_count_;

        return ref_count_;
    }

    uint32_t DecRefCount()
    {
        --ref_count_;

        return ref_count_;
    }

    uint8_t* GetPtr()
    {
        return ptr_;
    }

private:
    uint8_t* ptr_;
    atomic<uint32_t> ref_count_;
};

class Payload
{
public:
    Payload()
        :
        ref_ptr_(NULL),
        len_(0),
        frame_type_(kUnknownFrame)
    {
    }

    Payload(uint8_t* ptr, const uint64_t& len)
        :
        ref_ptr_(new RefPtr(ptr)),
        len_(len),
        frame_type_(kUnknownFrame)
    {
    }

    void SetIFrame() { frame_type_ = kIframe; }
    void SetBFrame() { frame_type_ = kBframe; }
    void SetPFrame() { frame_type_ = kPframe; }

    bool IsIFrame() { return frame_type_ == kIframe; }
    bool IsBFrame() { return frame_type_ == kBframe; }
    bool IsPFrame() { return frame_type_ == kPframe; }

    void SetPts(const uint64_t& pts) { pts_ = pts; }
    void SetDts(const uint64_t& dts) { dts_ = dts; }

    uint32_t GetPts() { return pts_; }
    uint32_t GetDts() { return dts_; }

    uint8_t GetFrameType() { return frame_type_; }

    void Reset(uint8_t* ptr, const uint64_t& len)
    {
        if (ref_ptr_ != NULL)
        {
            uint32_t referenct_count = ref_ptr_->DecRefCount();

            if (referenct_count == 0)
            {
                delete ref_ptr_;
            }
        }
        else
        {
            ref_ptr_ = new RefPtr(ptr);
            len_ = len;
        }
    }

    ~Payload()
    {
        uint32_t referenct_count = ref_ptr_->DecRefCount();

        if (referenct_count == 0)
        {
            delete ref_ptr_;
        }
    }

    Payload(const Payload& other)
    {
        if (this != &other)
        {
            this->ref_ptr_ = other.ref_ptr_;
            ref_ptr_->AddRefCount();
            this->len_ = other.len_;
            this->pts_ = other.pts_;
            this->dts_ = other.dts_;
            this->frame_type_ = other.frame_type_;
        }
    }

    Payload& operator=(const Payload& other)
    {
        if (this != &other)
        {
            this->ref_ptr_ = other.ref_ptr_;
            ref_ptr_->AddRefCount();
            this->len_ = other.len_;
            this->pts_ = other.pts_;
            this->dts_ = other.dts_;
            this->frame_type_ = other.frame_type_;
        }
    }

    uint8_t* GetPtr()
    {
        if (ref_ptr_ == NULL)
        {
            return NULL;
        }

        return ref_ptr_->GetPtr();
    }

    uint64_t GetLen()
    {
        return len_;
    }

    void AddLen(const uint64_t& delta)
    {
        len_ += delta;
    }
    
private:
    RefPtr* ref_ptr_;

    uint64_t len_;
	uint8_t frame_type_;
    uint64_t pts_;
    uint64_t dts_;
};

#endif // __REF_PTR_H__
