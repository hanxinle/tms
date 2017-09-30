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
#ifdef DEBUG
            cout << LMSG << "free ptr_:" << (void*)ptr_ << endl;
#endif
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
        is_key_frame_(false)
    {
    }

    void SetIFrame()
    {
        frame_type_ = kIframe;
    }

    void SetBFrame()
    {
        frame_type_ = kBframe;
    }

    void SetPFrame()
    {
        frame_type_ = kPframe;
    }

    bool IsIFrame()
    {
        return frame_type_ == kIframe;
    }

    bool IsBFrame()
    {
        return frame_type_ == kBframe;
    }

    bool IsPFrame()
    {
        return frame_type_ == kPframe;
    }

    void SetTimestamp(const uint32_t& timestamp)
    {
        timestamp_ = timestamp;
    }

    void SetKeyFrame()
    {
        is_key_frame_ = true;
    }

    bool IsKeyFrame()
    {
        return is_key_frame_;
    }

    uint32_t GetTimestamp()
    {
        return timestamp_;
    }

    void Reset(uint8_t* ptr, const uint64_t& len)
    {
        if (ref_ptr_ != NULL)
        {
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

#ifdef DEBUG
        cout << LMSG << "referenct count:" << referenct_count << endl;
#endif

        if (referenct_count == 0)
        {
#ifdef DEBUG
            cout << LMSG << "now delete ref_ptr_:" << (void*)ref_ptr_ << endl;
#endif
            delete ref_ptr_;
        }
    }

    Payload(const Payload& other)
    {
        if (this != &other)
        {
            this->ref_ptr_ = other.ref_ptr_;
            uint32_t referenct_count = ref_ptr_->AddRefCount();
            this->len_ = other.len_;
            this->timestamp_ = other.timestamp_;
            this->is_key_frame_ = other.is_key_frame_;
            this->frame_type_ = other.frame_type_;

#ifdef DEBUG
            cout << LMSG << "referenct count:" << referenct_count << endl;
#endif
        }
    }

    Payload& operator=(const Payload& other)
    {
        if (this != &other)
        {
            this->ref_ptr_ = other.ref_ptr_;
            uint32_t referenct_count = ref_ptr_->AddRefCount();
            this->len_ = other.len_;
            this->timestamp_ = other.timestamp_;
            this->is_key_frame_ = other.is_key_frame_;
            this->frame_type_ = other.frame_type_;

#ifdef DEBUG
            cout << LMSG << "referenct count:" << referenct_count << endl;
#endif
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

    uint8_t* GetLenPtr()
    {
        if (GetPtr() == NULL)
        {
            return NULL;
        }

        return GetPtr() + len_;
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
    uint32_t timestamp_;
    bool is_key_frame_;
    uint8_t frame_type_;
};


#endif // __REF_PTR_H__
