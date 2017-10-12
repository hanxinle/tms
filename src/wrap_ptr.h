#ifndef __WRAP_PTR_H__
#define __WRAP_PTR_H__

#include "common_define.h"

class WrapPtr
{
public:
    WrapPtr()
        :
        data_(NULL),
        len_(0),
        pts_(0),
        dts_(0),
        frame_type_(kUnknownFrame)
    {
    }

    WrapPtr(const uint8_t* data, const size_t& len)
        :
        data_(data),
        len_(len),
        pts_(0),
        dts_(0),
        frame_type_(kUnknownFrame)
    {
    }

    WrapPtr(const WrapPtr& other)
    {
        this->data_ = other.data_;
        this->len_  = other.len_;
        this->frame_type_ = other.frame_type_;
        this->pts_ = other.pts_;
        this->dts_ = other.dts_;
    }

    WrapPtr& operator=(const WrapPtr& other)
    {
        this->data_ = other.data_;
        this->len_  = other.len_;
        this->frame_type_ = other.frame_type_;
        this->pts_ = other.pts_;
        this->dts_ = other.dts_;

        return *this;
    }

    const uint8_t* GetPtr() const
    {
        return data_;
    }

    size_t GetLen() const
    {
        return len_;
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

    uint8_t GetFrameType()
    {
        return frame_type_;
    }

    void SetDts(const uint32_t& dts)
    {
        dts_ = dts;
    }

    uint32_t GetDts()
    {
        return dts_;
    }

    void SetPts(const uint32_t& pts)
    {
        pts_ = pts;
    }

    uint32_t GetPts()
    {
        return pts_;
    }

private:
    const uint8_t* data_;
    size_t len_;
        
    uint8_t frame_type_;
    uint32_t pts_;
    uint32_t dts_;
};

#endif // __WRAP_PTR_H__
