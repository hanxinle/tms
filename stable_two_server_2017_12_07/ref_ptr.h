#ifndef __REF_PTR_H__
#define __REF_PTR_H__

#include <assert.h>

#include <atomic>
#include <iostream>

#include "common_define.h"
#include "trace_tool.h"

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
            //cout << LMSG << "free " << (void*)ptr_ << endl;
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
        frame_type_(kUnknownFrame),
        payload_type_(kUnknownPayload)
    {
    }

    Payload(uint8_t* ptr, const uint64_t& len)
        :
        ref_ptr_(new RefPtr(ptr)),
        len_(len),
        frame_type_(kUnknownFrame),
        payload_type_(kUnknownPayload)
    {
    }

    uint32_t GetMask() const
    {
        uint32_t mask = 0;
        if (IsAudio())
        {
            MaskAudio(mask);
        }
        else if (IsVideo())
        {
            MaskVideo(mask);
        }

        if (IsPFrame())
        {
            MaskPFrame(mask);
        }
        else if (IsIFrame())
        {
            MaskIFrame(mask);
        }
        else if (IsBFrame())
        {
            MaskBFrame(mask);
        }

        return mask;
    }

    void SetIFrame() { frame_type_ = kIframe; }
    void SetBFrame() { frame_type_ = kBframe; }
    void SetPFrame() { frame_type_ = kPframe; }

    bool IsIFrame() const { return frame_type_ == kIframe; }
    bool IsBFrame() const { return frame_type_ == kBframe; }
    bool IsPFrame() const { return frame_type_ == kPframe; }

    void SetPts(const uint64_t& pts) { pts_ = pts; }
    void SetDts(const uint64_t& dts) { dts_ = dts; }

    uint64_t GetPts() const { return pts_; }
    uint64_t GetDts() const { return dts_; }

    uint32_t GetPts32() const { return (uint32_t)pts_; }
    uint32_t GetDts32() const { return (uint32_t)dts_; }

    uint8_t GetFrameType() { return frame_type_; }

    void SetAudio() { payload_type_ = kAudioPayload; }
    void SetVideo() { payload_type_ = kVideoPayload; }

    bool IsAudio() const { return payload_type_ == kAudioPayload; }
    bool IsVideo() const { return payload_type_ == kVideoPayload; }

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
        if (ref_ptr_ != NULL)
        {
            uint32_t referenct_count = ref_ptr_->DecRefCount();

            if (referenct_count == 0)
            {
                //cout << LMSG << "len:" << len_ << ",ref_ptr_:" << ref_ptr_ << endl;
                delete ref_ptr_;
            }
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
            this->payload_type_ = other.payload_type_;
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
            this->payload_type_ = other.payload_type_;
        }
    }

    uint8_t* GetAllData() const
    {
        return GetPtr();
    }

    uint64_t GetAllLen() const
    {
        return len_;
    }

    uint8_t* GetRawData() const
    {
        if (GetAllData() == NULL)
        {
            return NULL;
        }

        if (IsAudio())
        {
            return GetAllData() + 2;
        }
        else if (IsVideo())
        {
            return GetAllData() + 4;
        }

        return NULL;
    }

    uint64_t GetRawLen() const
    {
        if (IsAudio())
        {
            return GetAllLen() - 2;
        }
        else if (IsVideo())
        {
            return GetAllLen() - 4;
        }

        return 0;
    }

    void AddLen(const uint64_t& delta)
    {
        len_ += delta;
    }

private:

    uint8_t* GetPtr() const
    {
        if (ref_ptr_ == NULL)
        {
            return NULL;
        }

        return ref_ptr_->GetPtr();
    }
    
private:
    RefPtr* ref_ptr_;

    uint64_t len_;
	uint8_t frame_type_;
    uint8_t payload_type_;
    uint64_t pts_;
    uint64_t dts_;
};

#endif // __REF_PTR_H__
