#include <string.h>

#include <iostream>

#include "dh_tool.h"
#include "common_define.h"

#define BigNumber_1024 \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381" \
    "FFFFFFFFFFFFFFFF"

DhTool::DhTool()
    : dh_(NULL)
    , shared_key_(NULL)
    , shared_key_length_(0)
    , peer_public_key_(NULL)
{
}

DhTool::~DhTool()
{
    if (dh_ != NULL)
    {
        if (dh_->p != NULL)
        {
            BN_free(dh_->p);
            dh_->p = NULL;
        }
        if (dh_->g != NULL)
        {
            BN_free(dh_->g);
            dh_->g = NULL;
        }

        DH_free(dh_);
        dh_ = NULL;
    }

    if (shared_key_ != NULL)
    {
        delete shared_key_;
        shared_key_ = NULL;
    }

    if (peer_public_key_ != NULL)
    {
        BN_free(peer_public_key_);
        peer_public_key_ = NULL;
    }
}

int DhTool::Initialize(const uint32_t& bit_count)
{
    dh_ = DH_new();

    if (dh_ == NULL)
    {
        std::cout << LMSG << "DH_new failed" << std::endl;
        return -1;
    }

    dh_->p = BN_new();
    if (dh_->p == NULL)
    {
        std::cout << LMSG << "BN_new failed" << std::endl;
        return -1;
    }

    dh_->g = BN_new();
    if (dh_->g == NULL)
    {
        std::cout << LMSG << "BN_new failed" << std::endl;
        return -1;
    }

    if (BN_hex2bn(&dh_->p, BigNumber_1024) == 0)
    {
        std::cout << LMSG << "BN_hex2bn failed" << std::endl;
        return -1;
    }

    if (BN_set_word(dh_->g, 2) != 1)
    {
        std::cout << LMSG << "BN_set_word failed" << std::endl;
        return -1;
    }

    dh_->length = bit_count;

    if (DH_generate_key(dh_) != 1)
    {
        std::cout << LMSG << "DH_generate_key failed" << std::endl;
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
        std::cout << LMSG << "invalid shared_key_length_:" << shared_key_length_ << std::endl;
        return -1;
    }

    shared_key_ = new uint8_t[shared_key_length_];
    memset(shared_key_, 0, shared_key_length_);

    peer_public_key_ = BN_bin2bn(peer_public_key, peer_public_key_length, 0);
    if (peer_public_key_ == NULL)
    {
        std::cout << LMSG << "BN_bin2bn failed"<< std::endl;
        return -1;
    }

    if (DH_compute_key(shared_key_, peer_public_key_, dh_) == -1)
    {
        std::cout << LMSG << "DH_compute_key failed" << std::endl;
        return -1;
    }

    return 0;
}

int DhTool::CopyPublicKey(uint8_t* dst, const uint32_t& length)
{
    if (dh_ == NULL)
    {
        return -1;
    }

    if (length != shared_key_length_)
    {
        return -1;
    }

    int32_t key_size = BN_num_bytes(dh_->pub_key);

    if (BN_bn2bin(dh_->pub_key, dst) != key_size)
    {
        std::cout << LMSG << "BN_bn2bin failed" << std::endl;
        return -1;
    }

    return 0;
}
