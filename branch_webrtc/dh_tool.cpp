#include <string.h>

#include "dh_tool.h"

#define kP1024 \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381" \
    "FFFFFFFFFFFFFFFF"

DhTool::DhTool()
{
}

DhTool::~DhTool()
{
}

int DhTool::Initialize(const uint32_t& bit_count)
{
    dh_ = DH_new();

    if (dh_ == NULL)
    {
        return -1;
    }

    dh_->p = BN_new();
    if (dh_->p == NULL)
    {
        return -1;
    }

    dh_->g = BN_new();
    if (dh_->g == NULL)
    {
        return -1;
    }

    if (BN_hex2bn(&dh_->p, kP1024) == 0)
    {
        return -1;
    }

    if (BN_set_word(dh_->g, 2) != 1)
    {
        return -1;
    }

    dh_->length = bit_count;

    if (DH_generate_key(dh_) != 1)
    {
        return -1;
    }

    return 0;
}

int DhTool::CreateSharedKey(uint8_t* peer_public_key, const int32_t& peer_public_key_length)
{
    if (dh_ == NULL)
    {
        return -1;
    }

    shared_key_length_ = DH_size(dh_);

    if (shared_key_length_ <= 0 || shared_key_length_ > 1024)
    {
        return -1;
    }

    shared_key_ = new uint8_t[shared_key_length_];
    memset(shared_key_, 0, shared_key_length_);

    peer_public_key_ = BN_bin2bn(peer_public_key, peer_public_key_length, 0);
    if (peer_public_key_ == NULL)
    {
        return -1;
    }

    if (DH_compute_key(shared_key_, peer_public_key_, dh_) == -1)
    {
        return -1;
    }

    return 0;
}

int DhTool::CopyPublishKey(uint8_t* dst, const uint32_t& length)
{
    if (dh_ == NULL)
    {
        return -1;
    }

    if (length != shared_key_length_)
    {
        return -1;
    }

    memcpy(dst, shared_key_, shared_key_length_);

    return 0;
}
