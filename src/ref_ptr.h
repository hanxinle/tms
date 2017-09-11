#ifndef __REF_PTR_H__
#define __REF_PTR_H__

#include <assert.h>

#include <atomic>
#include <iostream>

#include "common_define.h"

using std::atomic;
using std::cout;
using std::endl;

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
            cout << LMSG << "free ptr_:" << (void*)ptr_ << endl;
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
        len_(0)
    {
    }

    Payload(uint8_t* ptr, const uint64_t& len)
        :
        ref_ptr_(new RefPtr(ptr)),
        len_(len)
    {
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

        cout << LMSG << "referenct count:" << referenct_count << endl;

        if (referenct_count == 0)
        {
            cout << LMSG << "now delete ref_ptr_:" << (void*)ref_ptr_ << endl;
            delete ref_ptr_;
        }
    }

    Payload(const Payload& other)
    {
        if (this != &other)
        {
            this->ref_ptr_ = other.ref_ptr_;
            uint32_t referenct_count = ref_ptr_->AddRefCount();

            cout << LMSG << "referenct count:" << referenct_count << endl;
        }
    }

    Payload& operator=(const Payload& other)
    {
        if (this != &other)
        {
            this->ref_ptr_ = other.ref_ptr_;
            uint32_t referenct_count = ref_ptr_->AddRefCount();

            cout << LMSG << "referenct count:" << referenct_count << endl;
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
};


#endif // __REF_PTR_H__
