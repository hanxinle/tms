#ifndef __DH_TOOL_H__
#define __DH_TOOL_H__

#include <stdint.h>

#include "openssl/bn.h"
#include "openssl/dh.h"

class DhTool
{
public:
    DhTool();
    ~DhTool();

    int Initialize(const uint32_t& bit_count);
    int CreateSharedKey(uint8_t* peer_public_key, const int32_t& peer_public_key_length);
    int CopyPublishKey(uint8_t* dst, const uint32_t& length);

private:
    DH* dh_;
    uint8_t* shared_key_;
    uint32_t shared_key_length_;
	BIGNUM* peer_public_key_;
};

#endif // __DH_TOOL_H__
