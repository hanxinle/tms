#ifndef __FAST_MAP_H__
#define __FAST_MAP_H__

#include <stddef.h>
#include <stdint.h>

#include "ref_ptr.h"

class FastMap
{
public:
    FastMap(const size_t capacity);
    ~FastMap();

    bool Insert(const uint64_t& index, const Payload& payload);

    struct Iterator
    {
        Iterator()
            :
            key_(NULL),
            value_(NULL)
        {
        }

        Iterator(const uint64_t* key, Payload* value)
            :
            key_(key),
            value_(value)
        {
        }

        const uint64_t first() const
        {
            return *key_;
        }

        Payload& second()
        {
            return *value_;
        }

        const uint64_t* key_;
        Payload* value_;
    };

    Iterator begin();
    Iterator end()
    {
        return iter_end_;
    }
    
private:
    Payload* payload_array_;
    uint64_t* help_array_;
    size_t capacity_;
    size_t size_;
    int pre_index_;

    Iterator iter_end_;
};

#endif // __FAST_MAP_H__
