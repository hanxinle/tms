#include <stdlib.h>
#include <string.h>

#include "fast_map.h"

FastMap::FastMap(const size_t capacity)
    :
    capacity_(capacity),
    size_(0),
    pre_index_(-1)
{
    payload_array_ = (Payload*)malloc(sizeof(Payload)*size_);
    help_array_ = (uint64_t*)malloc(sizeof(uint64_t)*size_);

    memset(payload_array_, 0, sizeof(Payload)*size_);
    memset(help_array_, 0, sizeof(uint64_t)*size_);
}

FastMap::~FastMap()
{
}

bool FastMap::Insert(const uint64_t& index, const Payload& payload)
{
    payload_array_[index % capacity_] = payload;
    help_array_[index % capacity_] = index;

    pre_index_ = index % capacity_;

    return true;
}

FastMap::Iterator FastMap::begin()
{
    if (pre_index_ < 0)
    {
        return end();
    }
}
