#include <math.h>

#include <deque>
#include <iostream>

#include "amf_0.h"
#include "any.h"
#include "assert.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "common_define.h"
#include "crc32.h"
#include "dh_tool.h"
#include "global.h"
#include "http_flv_protocol.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "protocol_factory.h"
#include "rtmp_protocol.h"
#include "fd.h"
#include "tcp_socket.h"
#include "util.h"

extern LocalStreamCenter g_local_stream_center;

static uint32_t s0_len = 1;
static uint32_t s1_len = 4/*time*/ + 4/*zero*/ + 1528/*random*/;
static uint32_t s2_len = 4/*time*/ + 4/*time2*/ + 1528/*random*/;

static uint8_t kFlashMediaServerKey[] = 
{
	0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20,
    0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
    0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69,
    0x61, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
    0x20, 0x30, 0x30, 0x31, 0xf0, 0xee, 0xc2, 0x4a, 
	0x80, 0x68, 0xbe, 0xe8, 0x2e, 0x00, 0xd0, 0xd1, 
	0x02, 0x9e, 0x7e, 0x57, 0x6e, 0xec, 0x5d, 0x2d, 
	0x29, 0x80, 0x6f, 0xab, 0x93, 0xb8, 0xe6, 0x36, 
	0xcf, 0xeb, 0x31, 0xae
};

static uint8_t kFlashPlayerKey[] = 
{
   	0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20,
    0x41, 0x64, 0x6F, 0x62, 0x65, 0x20, 0x46, 0x6C,
    0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79, 
    0x65, 0x72, 0x20, 0x30, 0x30, 0x31, 0xF0, 0xEE, 
    0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 
    0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57, 0x6E, 0xEC, 
    0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8,
    0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE 
};

static int HmacEncode(const std::string& algo, const uint8_t* key, const int& key_length,  
                      const uint8_t* input, const int& input_length,  
                      uint8_t* output, unsigned int& output_length) 
{  
    const EVP_MD* engine = NULL;

    if (algo == "sha512") 
    {    
        engine = EVP_sha512();  
    }    
    else if(algo == "sha256") 
    {    
        engine = EVP_sha256();  
    }    
    else if(algo == "sha1") 
    {    
        engine = EVP_sha1();  
    }    
    else if(algo == "md5") 
    {    
        engine = EVP_md5();  
    }    
    else if(algo == "sha224") 
    {    
        engine = EVP_sha224();  
    }    
    else if(algo == "sha384") 
    {    
        engine = EVP_sha384();  
    }    
    else if(algo == "sha") 
    {    
        engine = EVP_sha();  
    }    
    else 
    {    
        std::cout << LMSG << "Algorithm " << algo << " is not supported by this program!" << std::endl;  
        return -1;  
    }    
  
    HMAC_CTX ctx;  
    HMAC_CTX_init(&ctx);  
    HMAC_Init_ex(&ctx, key, key_length, engine, NULL);  
    HMAC_Update(&ctx, input, input_length);
  
    HMAC_Final(&ctx, output, &output_length);  
    HMAC_CTX_cleanup(&ctx);  
  
    return 0;   
}

RtmpProtocol::RtmpProtocol(IoLoop* io_loop, Fd* fd)
    : MediaPublisher()
    , MediaSubscriber(kRtmp)
    , io_loop_(io_loop)
    , socket_(fd)
    , handshake_status_(kStatus_0)
    , role_(RtmpRole::kUnknownRtmpRole)
    , version_(3)
    , scheme_(0)
    , encrypted_(false)
    , in_chunk_size_(128)
    , out_chunk_size_(128)
    , transaction_id_(0.0)
    , can_publish_(false)
{
    std::cout << LMSG << std::endl;
}

RtmpProtocol::~RtmpProtocol()
{
    std::cout << LMSG << std::endl;
}

int RtmpProtocol::HandleRead(IoBuffer& io_buffer, Fd& socket)
{ 
    int ret = kError;
    do
    {
        ret = Parse(io_buffer);
    } while (ret == kSuccess);

    return ret;
}

int RtmpProtocol::ParseRtmpUrl(const std::string& url, RtmpUrl& rtmp_url)
{
    size_t pre_pos = 0;
    auto pos = url.find("rtmp://");

    if (pos == std::string::npos)
    {
        return -1;
    }

    pos += 7;
    pre_pos = pos;

    pos = url.find("/", pre_pos);

    if (pos == std::string::npos)
    {
        return -1;
    }

    std::string ip_port = url.substr(pre_pos, pos - pre_pos);

    std::string ip;
    uint16_t port;
    {
        auto pos = ip_port.find(":");
        if (pos == std::string::npos)
        {
            ip = ip_port;
            port = 1935;
        }
        else
        {
            ip = ip_port.substr(0, pos);
            port = Util::Str2Num<uint16_t>(ip_port.substr(pos + 1));
        }
    }

    pos += 1;
    pre_pos = pos;

    pos = url.find("/", pre_pos);

    if (pos == std::string::npos)
    {
        return -1;
    }

    std::string app = url.substr(pre_pos, pos - pre_pos);

    pos += 1;
    pre_pos = pos;
    
    pos = url.find("?", pre_pos);

    std::string stream = "";
    if (pos == std::string::npos)
    {
        stream = url.substr(pre_pos);
    }
    else
    {
        stream = url.substr(pre_pos, pos - pre_pos);
    }

    if (stream.empty())
    {
        return -1;
    }

    pos += 1;
    pre_pos = pos;

    std::string args_str = url.substr(pos);

    std::vector<std::string> kv_vec = Util::SepStr(args_str, "&");
    std::map<std::string, std::string> args;

    std::ostringstream os;
    for (const auto& item : kv_vec)
    {
        std::vector<std::string> kv = Util::SepStr(item, "=");

        os << "(" << item << ") ";

        if (kv.size() == 2)
        {
            args[kv[0]] = kv[1];
        }
    }

    rtmp_url.ip = ip;
    rtmp_url.port = port;
    rtmp_url.app = app;
    rtmp_url.stream = stream;
    rtmp_url.args = args;

    std::cout << LMSG << "ip:" << ip << ",port:" << port << ",app:" << app << ",stream:" << stream << ",args:" << os.str() << std::endl;

    return 0;
}

uint32_t RtmpProtocol::GetDigestOffset(const uint8_t scheme, const uint8_t* buf)
{
    uint32_t offset = 0;
    if (scheme == 0)
    {
        offset = buf[8] + buf[9] + buf[10] + buf[11];
        offset = offset % 728;
        offset = offset + 12;

        if (offset + 32 >= 1536)
        {
            std::cout << LMSG << "invalid offset:" << offset << std::endl;
        }
    }
    else if (scheme == 1)
    {
        offset = buf[772] + buf[773] + buf[774] + buf[775];
        offset = offset % 728;
        offset = offset + 776;

        if (offset + 32 >= 1536)
        {
            std::cout << LMSG << "invalid offset:" << offset << std::endl;
        }
    }

    std::cout << LMSG << "scheme:" << (int)scheme << ",offset:" << offset << std::endl;

    return offset;
}

uint32_t RtmpProtocol::GetKeyOffset(const uint8_t& scheme, const uint8_t* buf)
{
    uint32_t offset = 0;
    if (scheme == 0)
    {
        offset = buf[1532] + buf[1533] + buf[1534] + buf[1535];
        offset = offset % 632;
        offset = offset + 772;

        if (offset + 128 >= 1536)
        {
            std::cout << LMSG << "invalid offset:" << offset << std::endl;
        }
    }
    else if (scheme == 1)
    {
        offset = buf[768] + buf[769] + buf[770] + buf[771];
        offset = offset % 632;
        offset = offset + 8;

        if (offset + 128 >= 1536)
        {
            std::cout << LMSG << "invalid offset:" << offset << std::endl;
        }
    }

    std::cout << LMSG << "scheme:" << (int)scheme << ",offset:" << offset << std::endl;

    return offset;
}

bool RtmpProtocol::GuessScheme(const uint8_t& scheme, const uint8_t* buf)
{
    std::cout << LMSG << "s1:\n" << Util::Bin2Hex(buf, 1536) << std::endl;
    std::cout << LMSG << "scheme:" << (int)scheme << std::endl;
    uint32_t client_digest_offset = GetDigestOffset(scheme, buf);

    uint8_t cal_buf[1536 - 32] = {0};

    memcpy(cal_buf, buf, client_digest_offset);
    memcpy(cal_buf + client_digest_offset, buf + client_digest_offset + 32, 1536 - client_digest_offset - 32);

    std::cout << LMSG << "cal_buf:\n" << Util::Bin2Hex(cal_buf, 1536 - 32) << std::endl;

    uint8_t sha256[256] = {0};
    uint8_t* p_sha256 = sha256;
    unsigned int sha256_out_len = 0;
    HmacEncode("sha256", kFlashPlayerKey, 30, cal_buf, sizeof(cal_buf), p_sha256, sha256_out_len);

    std::cout << LMSG << "sha256_out_len:" << sha256_out_len << std::endl;
    std::cout << LMSG << Util::Bin2Hex(sha256, 32) << std::endl;
    std::cout << LMSG << Util::Bin2Hex(buf + client_digest_offset, 32) << std::endl;

    return memcmp(sha256, buf + client_digest_offset, 32) == 0;
}

void RtmpProtocol::GenerateRandom(uint8_t* data, const int& len)
{
    for (int i = 0; i < len; ++i)
    {
        data[i] = (uint8_t)(random() % 256);
    }
}

int RtmpProtocol::Parse(IoBuffer& io_buffer)
{
    if (IsHandshakeDone())
    {
        bool one_message_done = false;
        uint32_t cs_id = 0;

        if (io_buffer.Size() >= 1)
        {
            uint8_t* buf = NULL;
            io_buffer.Peek(buf, 0, 1);

            BitBuffer bit_buffer(buf, 1);
            uint8_t fmt = 0;

            uint16_t chunk_header_len = 1;
            uint32_t message_header_len = 0;

            bit_buffer.GetBits(2, fmt);
            bit_buffer.GetBits(6, cs_id);

            if (cs_id == 0)
            {
                if (io_buffer.Size() >= 2)
                {
                    io_buffer.Peek(buf, 1, 1);
                    BitBuffer bit_buffer(buf, 1);

                    bit_buffer.GetBits(8, cs_id);
                    cs_id += 64;

                    chunk_header_len = 2;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (cs_id == 1)
            {
                if (io_buffer.Size() >= 3)
                {
                    io_buffer.Peek(buf, 1, 2);
                    BitBuffer bit_buffer(buf, 2);

                    bit_buffer.GetBits(16, cs_id);
                    cs_id += 64;

                    chunk_header_len = 3;
                }
                else
                {
                    return kNoEnoughData;
                }
            }

            if (fmt == 0)
            {
                message_header_len = 11;
            }
            else if (fmt == 1)
            {
                message_header_len = 7;
            }
            else if (fmt == 2)
            {
                message_header_len = 3;
            }
            else if (fmt == 3)
            {
                message_header_len = 0;
            }

            RtmpMessage& rtmp_msg = csid_head_[cs_id];
            if (fmt == 0)
            {
                if (io_buffer.Size() >= chunk_header_len + message_header_len)
                {
                    uint32_t timestamp = 0;
                    uint32_t message_length = 0;
                    uint8_t  message_type_id = 0;
                    uint32_t message_stream_id = 0;

                    io_buffer.Peek(buf, chunk_header_len, message_header_len);
                    BitBuffer bit_buffer(buf, message_header_len);

                    bit_buffer.GetBytes(3, timestamp);
                    bit_buffer.GetBytes(3, message_length);
                    bit_buffer.GetBytes(1, message_type_id);
                    bit_buffer.GetBytes(4, message_stream_id);

                    rtmp_msg.timestamp = timestamp;
                    rtmp_msg.timestamp_calc = timestamp;
                    rtmp_msg.message_length = message_length;
                    rtmp_msg.message_type_id = message_type_id;
                    rtmp_msg.message_stream_id = be32toh(message_stream_id);
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 1)
            {
                if (io_buffer.Size() >= chunk_header_len + message_header_len)
                {
                    uint32_t timestamp_delta = 0;
                    uint32_t message_length = 0;
                    uint8_t  message_type_id = 0;

                    io_buffer.Peek(buf, chunk_header_len, message_header_len);
                    BitBuffer bit_buffer(buf, message_header_len);

                    bit_buffer.GetBytes(3, timestamp_delta);
                    bit_buffer.GetBytes(3, message_length);
                    bit_buffer.GetBytes(1, message_type_id);

                    rtmp_msg.timestamp_delta = timestamp_delta;
                    rtmp_msg.message_length = message_length;
                    rtmp_msg.message_type_id = message_type_id;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 2)
            {
                if (io_buffer.Size() >= chunk_header_len + message_header_len)
                {
                    uint32_t timestamp_delta = 0;

                    io_buffer.Peek(buf, chunk_header_len, message_header_len);
                    BitBuffer bit_buffer(buf, message_header_len);

                    bit_buffer.GetBytes(message_header_len, timestamp_delta);

                    rtmp_msg.timestamp_delta = timestamp_delta;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (fmt == 3)
            {
            }

            if (io_buffer.Size() >= chunk_header_len + message_header_len)
            {
                uint32_t read_len = rtmp_msg.message_length - rtmp_msg.len;
                if (read_len > in_chunk_size_)
                {
                    read_len = in_chunk_size_;
                }

                if (io_buffer.Size() >= chunk_header_len + message_header_len + read_len)
                {
                    if (rtmp_msg.len == 0)
                    {
                        rtmp_msg.msg = (uint8_t*)malloc(rtmp_msg.message_length);
                    }

                    io_buffer.Skip(chunk_header_len + message_header_len);
                    io_buffer.ReadAndCopy(rtmp_msg.msg + rtmp_msg.len, read_len);

                    rtmp_msg.len += read_len;

                    if (rtmp_msg.len == rtmp_msg.message_length)
                    {
                        rtmp_msg.timestamp_calc += rtmp_msg.timestamp_delta;
                        one_message_done = true;
                    }
                    else
                    {
                        return kSuccess;
                    }
                }
                else
                {
                    return kNoEnoughData;
                }
            }
        }
        else
        {
            return kNoEnoughData;
        }

        if (one_message_done)
        {
            RtmpMessage& rtmp_msg = csid_head_[cs_id];
            rtmp_msg.cs_id = cs_id;

            int ret = OnRtmpMessage(rtmp_msg);

            free(rtmp_msg.msg);

            rtmp_msg.msg = NULL;
            rtmp_msg.len = 0;

            return ret;
        }
    }
    else
    {
        if (IsClientRole())
        {
            std::cout << LMSG << "handshake_status_:" << handshake_status_ << ",io_buffer.Size():" << io_buffer.Size() << std::endl;

            if (handshake_status_ == kStatus_2)
            {
                if (io_buffer.Size() >= s0_len + s1_len + s2_len)
                {
                    io_buffer.Skip(s0_len);

                    uint32_t time;

                    uint32_t client_time = Util::GetNowMs();

                    io_buffer.ReadU32(time);

                    uint8_t* buf = NULL;

                    io_buffer.Read(buf, s1_len - 4);
                    io_buffer.Skip(s2_len);

                    io_buffer.WriteU32(client_time);
                    io_buffer.WriteU32(time);
                    io_buffer.Write(buf, 1528);

                    io_buffer.Read(buf, s2_len);

                    socket_->Send(buf, s2_len);

                    handshake_status_ = kStatus_Done;

                    SetOutChunkSize(4096);

                    SendConnect("rtmp://" + domain_ + "/" + app_ + "/" + stream_);

                    return kSuccess;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else
            {
                std::cout << LMSG << "error" << std::endl;
                return kError;
            }
        }
        else
        {
            if (handshake_status_ == kStatus_0)
            {
                if (io_buffer.Size() >= s0_len)
                {
                    if (io_buffer.ReadU8(version_) == 1)
                    {
                        std::cout << LMSG << "version:" << (int)version_ << std::endl;
                        handshake_status_ = kStatus_1;
                        return kSuccess;
                    }
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (handshake_status_ == kStatus_1)
            {
                if (io_buffer.Size() >= s1_len)
                {
                    uint8_t* peek_buf = NULL;
                    std::cout << LMSG << io_buffer.Peek(peek_buf, 0, s1_len) << std::endl;

                    if (version_ == 3)
                    {
                        uint8_t* buf = NULL;
                        io_buffer.Read(buf, 4);

                        BitBuffer bit_buffer(buf, 4);

                        uint32_t timestamp;
                        bit_buffer.GetBytes(4, timestamp);
                        // send s0 + s1 + s2

                        uint32_t zero = 0;

                        io_buffer.ReadU32(zero);

                        if (zero == 0) // simple handshake
                        {
                            std::cout << LMSG << "simple handshake" << std::endl;
                            io_buffer.Read(buf, 1528);

                            // s0
                            io_buffer.WriteU8(version_);

                            // s1
                            uint32_t server_time = Util::GetNowMs();
                            io_buffer.WriteU32(server_time);

                            zero = 0;
                            io_buffer.WriteU32(zero);

                            io_buffer.WriteFake(1528);

                            // s2
                            io_buffer.WriteU32(timestamp);
                            io_buffer.WriteU32(server_time);
                            io_buffer.Write(buf, 1528);

                            io_buffer.WriteToFd(socket_->fd());

                            handshake_status_ = kStatus_2;
                            return kSuccess;
                        }
                        else // complex handshake
                        {
                            // 已经peek了,剩下的不要
                            io_buffer.Skip(1528);

                            std::cout << LMSG << "complex handshake" << std::endl;
                            bool guess_success = false;
                            for (int i = 0; i < 2; ++i)
                            {
                                // 解析scheme0和scheme1,判断客户端用的是哪种格式
                                bool scheme_guess = GuessScheme(i, peek_buf);
                                if (scheme_guess)
                                {
                                    scheme_ = i;
                                    guess_success = true;
                                    std::cout << LMSG << "use scheme " << scheme_ << std::endl;
                                    break;
                                }
                            }

                            if (! guess_success)
                            {
                                std::cout << LMSG << "scheme guess failed" << std::endl;
                                return kClose;
                            }

                            // complex handshake s0 + s1 + s2 response
                            /*
                                c1/s1: 1536 = 4 + 4 + 764 + 764 = 1536
                                
                                ------------------------
                                scheme 0
                                ------------------------
                                time     | 4 bytes
                                version  | 4 bytes
                                key      | 764 bytes
                                digest   | 764 bytes
                                ------------------------
                                scheme 1
                                ------------------------
                                time     | 4 bytes
                                version  | 4 bytes
                                digest   | 764 bytes
                                key      | 764 bytes
                                ------------------------
                                
                                -------------------------------------------------
                                key
                                -------------------------------------------------
                                random_data    |    offset bytes
                                key_data       |    128 bytes
                                random_data    |    (764-128-offset-4) bytes
                                offset         |    4bytes
                                -------------------------------------------------
                                digest
                                -------------------------------------------------
                                offset         |    4bytes
                                random_data    |    offset bytes
                                digest_data    |    32 bytes
                                random_data    |    (764-32-offset-4) bytes
                                -------------------------------------------------
                             */
                            {
                                // s1 response cal
                                uint8_t s1[1536];
                                GenerateRandom(s1, sizeof(s1));
                                // XXX: 写入 time version

                                uint32_t server_dh_offset = GetKeyOffset(scheme_, s1);
                                uint32_t client_dh_offset = GetKeyOffset(scheme_, peek_buf);

                                std::cout << LMSG << "server_dh_offset:" << server_dh_offset << ",client_dh_offset:" << client_dh_offset << std::endl;

                                DhTool dh_tool;
                                dh_tool.Initialize(1024);
                                dh_tool.CreateSharedKey(peek_buf + client_dh_offset, 128);
                                dh_tool.CopyPublicKey(s1 + server_dh_offset, 128);

                                uint32_t server_digest_offset = GetDigestOffset(scheme_, s1);

                                std::cout << LMSG << "server_digest_offset:" << server_digest_offset << std::endl;

                                uint8_t cal_buf[1536 - 32];

                                memcpy(cal_buf, s1, server_digest_offset);
                                memcpy(cal_buf + server_digest_offset, s1 + server_digest_offset + 32, 1536 - server_digest_offset - 32);

                                uint8_t* p_sha256 = s1 + server_digest_offset;
                                unsigned int sha256_out_len = 0;
                                HmacEncode("sha256", kFlashMediaServerKey/*key*/, 36/*key len*/, cal_buf/*data*/, sizeof(cal_buf)/*data len*/, p_sha256, sha256_out_len);

                                std::cout << LMSG << "server digest\n" << Util::Bin2Hex(p_sha256, sha256_out_len) << std::endl;
                                std::cout << LMSG << "s1\n" << Util::Bin2Hex(s1, s1_len) << std::endl;

                                // s1 response cal
                                uint32_t client_digest_offset = GetDigestOffset(scheme_, peek_buf);
                                std::cout << LMSG << "client_digest_offset:" << client_digest_offset << std::endl;
                                uint8_t client_digest_sha256[32] = {0};
                                // 将客户端的digest用kFlashMediaServerKey做一次sha256
                                HmacEncode("sha256", kFlashMediaServerKey/*key*/, 68/*key len*/, peek_buf + client_digest_offset/*data*/, 32/*data len*/, client_digest_sha256, sha256_out_len);

                                uint8_t s2[1536];
                                GenerateRandom(s2, sizeof(s2));

                                uint32_t second_hash[512] = {0};
                                // 将上面拿到的'客户端的digest用kFlashMediaServerKey做一次sha256'作为key,对S2前1536-32字节做一次sha256
                                HmacEncode("sha256", client_digest_sha256/*key*/, 32/*key len*/, s2/*data*/, 1536 - 32/*data len*/, s2 + 1536 - 32, sha256_out_len);

                                io_buffer.WriteU8(3);
                                io_buffer.Write(s1, s1_len);
                                io_buffer.Write(s2, s2_len);
                                io_buffer.WriteToFd(socket_->fd());

                                handshake_status_ = kStatus_2;
                                return kSuccess;
                            }

                        }
                    }
                    else if (version_ == 6) 
                    {
                        std::cout << LMSG << "encrypted" << std::endl;
                        encrypted_ = true;
                    }
                }
                else
                {
                    return kNoEnoughData;
                }
            }
            else if (handshake_status_ == kStatus_2)
            {
                if (io_buffer.Size() >= s2_len)
                {
                    uint8_t* buf = NULL;
                    io_buffer.Read(buf, 8);

                    BitBuffer bit_buffer(buf, 8);

                    uint32_t timestamp = 0;
                    bit_buffer.GetBytes(4, timestamp);

                    uint32_t timestamp2 = 0;
                    bit_buffer.GetBytes(4, timestamp2);

                    io_buffer.Read(buf, 1528);

                    handshake_status_ = kStatus_Done;

                    SetOutChunkSize(4096);

                    std::cout << LMSG << "Handshake done!!!" << std::endl;
                    return kSuccess;
                }
                else
                {
                    return kNoEnoughData;
                }
            }
        }
        //else
        //{
        //    std::cout << LMSG << "unknow rtmp role " << (int)role_ << std::endl;
        //}
    }

    assert(false);
    // avoid warning
    std::cout << LMSG << "error" << std::endl;
    return kError;
}

int RtmpProtocol::OnSetChunkSize(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t chunk_size = 0;
    bit_buffer.GetBytes(4, chunk_size);

    std::cout << LMSG << "chunk_size:" << in_chunk_size_ << "->" << chunk_size << std::endl;

    in_chunk_size_ = chunk_size;

    return kSuccess;
}

int RtmpProtocol::OnAcknowledgement(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t sequence_number = 0;
    bit_buffer.GetBytes(4, sequence_number);

    std::cout << LMSG << "sequence_number:" << sequence_number << std::endl;

    return kSuccess;
}

int RtmpProtocol::OnWindowAcknowledgementSize(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t ack_window_size = 0;
    bit_buffer.GetBytes(4, ack_window_size);

    std::cout << LMSG << "ack_window_size:" << ack_window_size << std::endl;

    SendUserControlMessage(0, 0);

    return kSuccess;
}

int RtmpProtocol::OnSetPeerBandwidth(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint32_t ack_window_size = 0;
    bit_buffer.GetBytes(4, ack_window_size);

    uint8_t limit_type = 0;
    bit_buffer.GetBytes(1, limit_type);

    std::cout << LMSG << "ack_window_size:" << ack_window_size
                 << ", limit_type:" << (int)limit_type 
                 << std::endl;

    return kSuccess;
}

int RtmpProtocol::OnUserControlMessage(RtmpMessage& rtmp_msg)
{
    BitBuffer bit_buffer(rtmp_msg.msg, rtmp_msg.len);

    uint16_t event = 0xff;
    bit_buffer.GetBytes(2, event);

    uint32_t data = 0;
    bit_buffer.GetBytes(4, data);

    std::cout << LMSG << "user control message, event:" << event << ",data:" << data << std::endl;

    return kSuccess;
}

int RtmpProtocol::OnAudio(RtmpMessage& rtmp_msg)
{
    //std::cout << LMSG << "timestamp:" << rtmp_msg.timestamp << ",timestamp_delta:" << rtmp_msg.timestamp_delta << ",timestamp_calc:" << rtmp_msg.timestamp_calc << std::endl;
    if (rtmp_msg.len >= 2)
    {
        BitBuffer bit_buffer(rtmp_msg.msg, 2);

        uint8_t sound_format = 0xff;
        uint8_t sound_rate = 0xff;
        uint8_t sound_size = 0xff;
        uint8_t sound_type = 0xff;
        uint8_t aac_packet_type = 0xff;

        bit_buffer.GetBits(4, sound_format);
        bit_buffer.GetBits(2, sound_rate);
        bit_buffer.GetBits(1, sound_size);
        bit_buffer.GetBits(1, sound_type);
        bit_buffer.GetBits(8, aac_packet_type);

        if (sound_format == 10)
        {
            if (aac_packet_type == 0)
            {
                std::string audio_header((const char*)rtmp_msg.msg + 2, rtmp_msg.len - 2);

                std::cout << LMSG << "recv audio_header,size:" << audio_header.size() << std::endl;
                std::cout << Util::Bin2Hex(audio_header) << std::endl;

                media_muxer_.OnAudioHeader(audio_header);

            }
            else
            {
                uint8_t* audio_raw_data = (uint8_t*)malloc(rtmp_msg.len);
                memcpy(audio_raw_data, rtmp_msg.msg, rtmp_msg.len);

                Payload audio_payload(audio_raw_data, rtmp_msg.len);
                audio_payload.SetAudio();
                audio_payload.SetDts(rtmp_msg.timestamp_calc);
                audio_payload.SetPts(rtmp_msg.timestamp_calc);

                media_muxer_.OnAudio(audio_payload);

                for (auto& sub : subscriber_)
                {
                    sub->SendMediaData(audio_payload);
                }
            }
        }
    }
    else
    {
        std::cout << LMSG << "impossible?" << std::endl;
        assert(false);
    }

    return kSuccess;
}

int RtmpProtocol::OnVideo(RtmpMessage& rtmp_msg)
{
    bool to_media_muxer = false;
    uint8_t frame_type = 0xff;
    uint8_t codec_id = 0xff;
    uint8_t avc_packet_type = 0xff;

    if (rtmp_msg.len >= 2)
    {
        BitBuffer bit_buffer(rtmp_msg.msg, 2);

        bit_buffer.GetBits(4, frame_type);
        bit_buffer.GetBits(4, codec_id);
        bit_buffer.GetBits(8, avc_packet_type);

        // H264/AVC
        if (codec_id == 7)
        {
            if (avc_packet_type == 0)
            {
                OnVideoHeader(rtmp_msg);
            }
            else
            {
                uint32_t compositio_time_offset = 0;
                if (rtmp_msg.len > 5)
                {
                    compositio_time_offset = (rtmp_msg.msg[2] << 16) | (rtmp_msg.msg[3] << 8) | (rtmp_msg.msg[4]);

                    uint8_t* data = rtmp_msg.msg + 5;
                    size_t raw_len = rtmp_msg.len - 5;

                    int got_picture = 0;

                    size_t cur_len = 0;
                    while (cur_len < raw_len)
                    {
                        uint32_t nalu_len = (data[cur_len]<<24) | (data[cur_len+1]<<16) | (data[cur_len+2]<<8) | (data[cur_len+3]);

                        if (nalu_len > raw_len)
                        {
                            std::cout << LMSG << "nalu_len:" << nalu_len << " > raw_len:" << raw_len;
                            break;
                        }

                        uint8_t nalu_header = data[cur_len+4];

                        uint8_t forbidden_zero_bit = (nalu_header & 0x80) >> 7;
                        UNUSED(forbidden_zero_bit);

                        uint8_t nal_ref_idc = (nalu_header & 0x60) >> 5;
                        uint8_t nalu_unit_type = (nalu_header & 0x1F);

                        // 4 bytes nalu_len也push,方便后面FLV/RTMP的处理
                        uint8_t* video_raw_data = (uint8_t*)malloc(nalu_len + 4);
                        memcpy(video_raw_data, data + cur_len, nalu_len + 4);

                        Payload video_payload(video_raw_data, nalu_len + 4);

                        //std::cout << LMSG << "nalu_unit_type:" << (int)nalu_unit_type << std::endl;
                        // SEI不能传给webrtc,不然会导致只能解码关键帧,其他帧都无法解码
                        if (nalu_unit_type != 6)
                        {
                            //g_webrtc_mgr->__DebugSendH264(video_raw_data + 4, nalu_len, rtmp_msg.timestamp_calc);
                        }

                        video_payload.SetVideo();
                        video_payload.SetDts(rtmp_msg.timestamp_calc);
                        video_payload.SetPts(rtmp_msg.timestamp_calc);

                        //std::cout << LMSG << "NALU type + 4byte payload peek:[" << Util::Bin2Hex(data+cur_len+4, 5) << std::endl;

                        if (nalu_unit_type == H264NalType_SEI)
                        {
                            //std::cout << LMSG << "SEI [" << Util::Bin2Hex(data + cur_len + 4, nalu_len) << "]" << std::endl;
                            to_media_muxer = false;
                        }
                        else if (nalu_unit_type == H264NalType_SPS)
                        {
                            std::cout << LMSG << "SPS [" << Util::Bin2Hex(data + cur_len + 4, nalu_len) << "]" << std::endl;
                        }
                        else if (nalu_unit_type == H264NalType_PPS)
                        {
                            std::cout << LMSG << "PPS [" << Util::Bin2Hex(data + cur_len + 4, nalu_len) << "]" << std::endl;
                        }
                        else if (nalu_unit_type == H264NalType_IDR_SLICE)
                        {
                            to_media_muxer = true;
                            std::cout << LMSG << "IDR" << std::endl;
                            video_payload.SetIFrame();
                            video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                        }
                        else if (nalu_unit_type == H264NalType_SLICE)
                        {
                            to_media_muxer = true;
                            video_payload.SetPts(rtmp_msg.timestamp_calc + compositio_time_offset);
                        }
                        else
                        {
                            to_media_muxer = false;
                        }

                        if (to_media_muxer)
                        {
                            media_muxer_.OnVideo(video_payload);

                            for (auto& sub : subscriber_)
                            {
                                sub->SendMediaData(video_payload);
                            }
                        }

                        cur_len += nalu_len + 4;
                    }

                    assert(cur_len == raw_len);
                }
            }
        }
    }
    else
    {
        std::cout << LMSG << "impossible?" << std::endl;
        assert(false);
    }

    return kSuccess;
}

int RtmpProtocol::OnAmf0Message(RtmpMessage& rtmp_msg)
{
    std::string amf((const char*)rtmp_msg.msg, rtmp_msg.len);

    AmfCommand amf_command;
    int ret = Amf0::Decode(amf,  amf_command);
    std::cout << LMSG << "ret:" << ret << ", amf_command.size():" <<  amf_command.size() << ",rtmp:" << rtmp_msg.ToString() << std::endl;

    for (size_t index = 0; index != amf_command.size(); ++index)
    {
        const auto& command = amf_command[index];

        if (command != NULL)
        {
            std::cout << LMSG << "v type:" << command->TypeStr() << std::endl;
        }
        else
        {
            std::cout << LMSG << "v NULL" << std::endl;
        }
    }

    if (ret == 0 &&  amf_command.size() >= 1)
    {
        std::string command = "";
        if ( amf_command[0]->GetString(command))
        {
            std::cout << LMSG << "recv [" << command << " command]" << std::endl;

            if (IsClientRole())
            {
                std::cout << LMSG << "command:" << command << ",last_send_command_:" << last_send_command_ << ",transaction_id_:" << transaction_id_ << std::endl;
            }

            if (command == "connect")
            {
                return OnConnectCommand(amf_command);
            }
            else if (command == "play")
            {
                return OnPlayCommand(rtmp_msg, amf_command);
            }
            else if (command == "publish")
            {
                return OnPublishCommand(rtmp_msg, amf_command);
            }
            else if (command == "createStream")
            {
                return OnCreateStreamCommand(rtmp_msg, amf_command);
            }
            else if (command == "releaseStream")
            {
            }
            else if (command == "FCPublish")
            {
            }
            else if (command == "deleteStream")
            {
            }
            else if (command == "FCUnpublish")
            {
            }
            else if (command == "onFCPublish")
            {
                // XXX:还有一个_result(FCPublish这种请求有2个回包,1个是onFCPublish, 1个是_result)
                //SendCreateStream();
            }
            else if (command == "_result")
            {
                return OnResultCommand(amf_command);
            }
            else if (command == "_error")
            {
            }
            else if (command == "onStatus")
            {
                return OnStatusCommand(amf_command);
            }
        }
        else
        {
            assert(false);
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnConnectCommand(AmfCommand& amf_command)
{
    double trans_id = 0;
    std::map<std::string, any::Any*> command_object;

    if (amf_command.size() >= 3)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            std::cout << LMSG << "transaction_id:" << trans_id << std::endl;
        }
        if (amf_command[2]->GetMap(command_object))
        {
            for (auto& kv : command_object)
            {
                if (kv.first == "app")
                {
                    std::string app;
                    if (kv.second->GetString(app))
                    {
                        auto pos = app.find("/");
                        if (pos != std::string::npos)
                        {
                            app = app.substr(0, pos);
                        }
                        std::cout << LMSG << "app = " << app << std::endl;
                        SetApp(app);
                    }
                }

                if (kv.first == "tcUrl")
                {
                    if (kv.second->GetString(tc_url_))
                    {
                        std::cout << LMSG << "tcUrl = " << tc_url_ << std::endl;

                        RtmpUrl rtmp_url;
                        ParseRtmpUrl(tc_url_, rtmp_url);

                        if (! rtmp_url.stream.empty())
                        {
                            SetStreamName(rtmp_url.stream);
                        }
                    }
                }
            }
        }

        if (! app_.empty())
        {
            any::String result("_result");
            any::Double transaction_id(trans_id);
            any::Map properties;

            any::String code("NetConnection.Connect.Success");
            any::Map information({{"code", (any::Any*)&code}});


            IoBuffer output;
            std::vector<any::Any*> connect_result = { 
                (any::Any*)&result, (any::Any*)&transaction_id, (any::Any*)&properties, (any::Any*)&information 
            };

            int ret = Amf0::Encode(connect_result, output);
            std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;
            if (ret == 0)
            {
                uint8_t* data = NULL;
                int len = output.Read(data, output.Size());

                if (data != NULL && len > 0)
                {
                    SetWindowAcknowledgementSize(16*1024*1024);
                    SetPeerBandwidth(0x10000000, 2);
                    SendUserControlMessage(0, 0);
                    SendRtmpMessage(2, 0, kAmf0Command, data, len);
                }
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnPlayCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command)
{
    SetClientPull();
    double trans_id = 0;

    if (amf_command.size() >= 4)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            std::cout << LMSG << "transaction_id:" << trans_id << std::endl;
        }

        if (stream_.empty())
        {
            if (amf_command[3]->GetString(stream_))
            {
                std::cout << LMSG << "stream:" << stream_ << std::endl;
            }
        }

        MediaPublisher* media_publisher = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

        if (media_publisher == NULL)
        {
            std::cout << LMSG << "no found app:" << app_ << ", stream_:" << stream_ << std::endl;
			return kError;
		}

        any::String on_status("onStatus");
        any::Double transaction_id(0.0);
        any::Null null;

        any::String code("NetStream.Play.Start");
        any::Map information({{"code", (any::Any*)&code}});

        IoBuffer output;
        std::vector<any::Any*> play_result = {(any::Any*)&on_status, (any::Any*)&transaction_id, (any::Any*)&null, (any::Any*)&information};
        int ret = Amf0::Encode(play_result, output);
        std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;
        if (ret == 0)
        {
            uint8_t* data = NULL;
            int len = output.Read(data, output.Size());

            if (data != NULL && len > 0)
            {
                SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
            }

            SetPublisher(media_publisher);
            media_publisher->AddSubscriber(this);
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnPublishCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command)
{
    SetClientPush();

    double trans_id = 0;

    if (amf_command.size() >= 5)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            std::cout << LMSG << "transaction_id:" << trans_id << std::endl;
        }

        if (stream_.empty())
        {
            std::string stream;
            if (amf_command[3]->GetString(stream))
            {
                std::cout << LMSG << "stream:" << stream << std::endl;

                auto args_pos = stream.find("?");
                if (args_pos != std::string::npos)
                {
                    stream = stream.substr(0, args_pos);
                }
                SetStreamName(stream);

				if (g_local_stream_center.RegisterStream(app_, stream_, this) == false)
                {
                    std::cout << LMSG << "error" << std::endl;
                    return kError;
                }
            }
        }
        else
        {
			if (g_local_stream_center.RegisterStream(app_, stream_, this) == false)
            {
                std::cout << LMSG << "app:" << app_ << ",stream:" << stream_ << " already register" << std::endl;
            }
        }

        any::String on_status("onStatus");
        any::Double transaction_id(0.0);
        any::Null null;

        any::String code("NetStream.Publish.Start");
        any::Map information({{"code", (any::Any*)&code}});

        IoBuffer output;
        std::vector<any::Any*> publish_result = {(any::Any*)&on_status, (any::Any*)&transaction_id, (any::Any*)&null, (any::Any*)&information};
        int ret = Amf0::Encode(publish_result, output);
        std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;
        if (ret == 0)
        {
            uint8_t* data = NULL;
            int len = output.Read(data, output.Size());

            if (data != NULL && len > 0)
            {
                SendUserControlMessage(0, 0);
                SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnCreateStreamCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command)
{
    double trans_id = 0;

    if (amf_command.size() >= 3)
    {
        if (amf_command[1]->GetDouble(trans_id))
        {
            std::cout << LMSG << "transaction_id:" << trans_id << std::endl;

            any::String result("_result");
            any::Double transaction_id(trans_id);
            any::Null command_object;
            any::Double stream_id(1.0);

            IoBuffer output;
            std::vector<any::Any*> create_stream_result = { 
                (any::Any*)&result, (any::Any*)&transaction_id, (any::Any*)&command_object, (any::Any*)&stream_id 
            };

            int ret = Amf0::Encode(create_stream_result, output);
            std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;
            if (ret == 0)
            {
                uint8_t* data = NULL;
                int len = output.Read(data, output.Size());

                if (data != NULL && len > 0)
                {
                    SendRtmpMessage(rtmp_msg.cs_id, rtmp_msg.message_stream_id, kAmf0Command, data, len);
                }
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnResultCommand(AmfCommand& amf_command)
{
    double transaction_id = 0.0;
    if (amf_command.size() >= 2 && amf_command[1]->GetDouble(transaction_id))
    {
        std::cout << LMSG << "in _result, transaction_id:" << transaction_id << std::endl;
    }

    if (id_command_.count(transaction_id))
    {
        std::cout << LMSG << DumpIdCommand() << std::endl;
        std::string pre_call = id_command_[transaction_id];
        std::cout << LMSG << "pre_call " << transaction_id << " [" << pre_call << "]" << std::endl;
        if (pre_call == "connect")
        {
            if (role_ == RtmpRole::kPushServer)
            {
                SendReleaseStream();
                SendFCPublish();
                SendCreateStream();
                SendCheckBw();
            }
            else if (role_ == RtmpRole::kPullServer)
            {
                SendCreateStream();
                std::cout << LMSG << "pull server" << std::endl;
            }
        }
        else if (pre_call == "releaseStream")
        {
        }
        else if (pre_call == "FCPublish")
        {
            //SendCreateStream();
        }
        else if (pre_call == "createStream")
        {
            double stream_id = 1.0;
            if (amf_command.size() >= 4)
            {

                if (amf_command[3] != NULL && amf_command[3]->GetDouble(stream_id))
                {
                    std::cout << LMSG << "stream_id:" << stream_id << std::endl;
                }
            }

            if (role_ == RtmpRole::kPushServer)
            {
                SendPublish(stream_id);
            }
            else if (role_ == RtmpRole::kPullServer)
            {
                std::cout << LMSG << "pull server" << std::endl;
                SendPlay(stream_id);
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnStatusCommand(AmfCommand& amf_command)
{
    UNUSED(amf_command);

    if (last_send_command_ == "publish")
    {
        if (! can_publish_)
        {
            can_publish_ = true;

            if (role_ == RtmpRole::kPushServer)
            {
                publisher_->AddSubscriber(this);
            }
            else if (role_ == RtmpRole::kClientPull)
            {
                publisher_->AddSubscriber(this);
            }
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnMetaData(RtmpMessage& rtmp_msg)
{
    std::string amf((const char*)rtmp_msg.msg, rtmp_msg.len);

    media_muxer_.OnMetaData(amf);

    AmfCommand amf_command;
    int ret = Amf0::Decode(amf,  amf_command);
    std::cout << LMSG << "ret:" << ret << ", amf_command.size():" <<  amf_command.size() << std::endl;

    for (size_t index = 0; index != amf_command.size(); ++index)
    {
        const auto& command = amf_command[index];

        if (command != NULL)
        {
            std::cout << LMSG << "v type:" << command->TypeStr() << std::endl;
        }
        else
        {
            std::cout << LMSG << "v NULL" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnVideoHeader(RtmpMessage& rtmp_msg)
{
    // webrtc test
#if 0
    {
        std::string video_header((const char*)rtmp_msg.msg + 5, rtmp_msg.len - 5);
        video_header.erase(0, 6);

        uint16_t sps_len = (uint8_t)(video_header[0]) << 8 | 
                           (uint8_t)(video_header[1]);

        video_header.erase(0, 2);

        std::string sps = video_header.substr(0, sps_len);

        std::string pps = video_header.substr(sps_len + 3);

        std::cout << "sps:" << Util::Bin2Hex(sps) << std::endl;
        std::cout << "pps:" << Util::Bin2Hex(pps) << std::endl;

        g_webrtc_mgr->__DebugSendH264((const uint8_t*)sps.data(), sps.size(), 0);
        g_webrtc_mgr->__DebugSendH264((const uint8_t*)pps.data(), pps.size(), 0);
    }
#endif

    std::string video_header((const char*)rtmp_msg.msg + 5, rtmp_msg.len - 5);

    std::cout << LMSG << "recv video_header" << ",size:" << video_header.size() << std::endl;
    std::cout << Util::Bin2Hex(video_header) << std::endl;

    return media_muxer_.OnVideoHeader(video_header);
}

int RtmpProtocol::OnRtmpMessage(RtmpMessage& rtmp_msg)
{
    if (IsClientRole())
    {
        std::cout << LMSG << rtmp_msg.ToString() << std::endl;
    }

    switch (rtmp_msg.message_type_id)
    {
        // FIXME:
        case 0:
        {
            return kSuccess;
        }
        break;

        case kSetChunkSize:
        {
            return OnSetChunkSize(rtmp_msg);
        }
        break;

        case kAcknowledgement:
        {
            return OnAcknowledgement(rtmp_msg);
        }
        break;

        case kWindowAcknowledgementSize:
        {
            return OnWindowAcknowledgementSize(rtmp_msg);
        }
        break;

        case kSetPeerBandwidth:
        {
            return OnSetPeerBandwidth(rtmp_msg);
        }
        break;

        case kUserControlMessage:
        {
            return OnUserControlMessage(rtmp_msg);
        }
        break;

        case kAudio:
        {
            return OnAudio(rtmp_msg);
        }
        break;

        case kVideo:
        {
            return OnVideo(rtmp_msg);
        }
        break;

        case kAmf0Command:
        {
            return OnAmf0Message(rtmp_msg);
        }
        break;

        case kMetaData_AMF3:
        {
            return kSuccess;
        }

        case kMetaData_AMF0:
        {
            return OnMetaData(rtmp_msg);
        }
        break;

        default: 
        {
            std::cout << LMSG << "message_type_id:" << (uint16_t)rtmp_msg.message_type_id << std::endl;
            return kError;
        }
        break;
    }

    std::cout << LMSG << "error" << std::endl;
    return kError;
}

int RtmpProtocol::HandleClose(IoBuffer& io_buffer, Fd& socket)
{
    UNUSED(io_buffer);
    UNUSED(socket);

    for (const auto& kv : csid_head_)
    {
        if (kv.second.msg != NULL)
        {
            free(kv.second.msg);
        }
    }

    std::cout << LMSG << "role:" << (int)role_ << std::endl;

    csid_head_.clear();

    if (role_ == RtmpRole::kClientPush)
    {
        for (auto& sub : subscriber_)
        {
            sub->OnStop();
        }

		g_local_stream_center.UnRegisterStream(app_, stream_, this);
    }
    else if (role_ == RtmpRole::kPushServer)
    {
        if (publisher_ != NULL)
        {
            std::cout << LMSG << "remove forward" << std::endl;
            publisher_->RemoveSubscriber(this);
        }
    }
    else if (role_ == RtmpRole::kClientPull)
    {
        if (publisher_ != NULL)
        {
            std::cout << LMSG << "remove player" << std::endl;
            publisher_->RemoveSubscriber(this);
        }
    }

    return kSuccess;
}

int RtmpProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    if (role_ == RtmpRole::kClientPush || role_ == RtmpRole::kPullServer)
    {
        media_muxer_.EveryNSecond(now_in_ms, interval, count);
    }

    std::cout << LMSG << "subscriber:" << subscriber_.size() << std::endl;

    return kSuccess;
}

/*
 * Protocol control messages SHOULD have message stream ID 0(called as
 * control stream) and chunk stream ID 2, and are sent with highest
 * priority.
 * Each protocol control message type has a fixed-size payload, and is
 * always sent in a single chunk.
 */
int RtmpProtocol::SendRtmpMessage(const uint32_t cs_id, const uint32_t& message_stream_id, const uint8_t& message_type_id, const uint8_t* data, const size_t& len)
{
    RtmpMessage rtmp_message;

    rtmp_message.cs_id = cs_id;
    rtmp_message.timestamp = 0;
    rtmp_message.timestamp_delta = 0;
    rtmp_message.message_length = len;
    rtmp_message.message_type_id = message_type_id;
    rtmp_message.message_stream_id = message_stream_id;

    rtmp_message.msg = (uint8_t*)data;
    rtmp_message.len = len;

    if (message_type_id == kAmf0Command)
    {
        return SendData(rtmp_message, Payload(), true);
    }
    return SendData(rtmp_message);
}

int RtmpProtocol::SendData(const RtmpMessage& cur_info, const Payload& payload, const bool& force_fmt0)
{
    const uint32_t cs_id = cur_info.cs_id;

    RtmpMessage& pre_info = csid_pre_info_[cs_id];

    uint32_t& pre_timestamp_delta = pre_info.timestamp_delta;
    uint32_t& pre_message_length  = pre_info.message_length;
    uint8_t&  pre_message_type_id = pre_info.message_type_id;

    uint32_t cur_timestamp_delta   = cur_info.timestamp_delta;
    uint32_t cur_message_length    = cur_info.message_length;
    uint32_t cur_message_stream_id = cur_info.message_stream_id;
    uint8_t  cur_message_type_id   = cur_info.message_type_id;

    int chunk_count = 1;
    if (cur_message_length > out_chunk_size_)
    {
        chunk_count = cur_message_length / out_chunk_size_;

        if (cur_message_length % out_chunk_size_ != 0)
        {
            chunk_count += 1;
        }
    }

    int fmt = 0x0f;

    // new cs_id, fmt0
    if (pre_message_length == 0 || force_fmt0)
    {
        fmt = 0;
    }
    else
    {
        if (pre_message_length == cur_message_length)
        {
            if (pre_message_type_id == cur_message_type_id)
            {
                if (pre_timestamp_delta == cur_timestamp_delta)
                {
                    fmt = 3;
                }
                else
                {
                    fmt = 2;
                }
            }
            else
            {
                fmt = 1;
            }
        }
        else
        {
            fmt = 1;
        }
    }

    assert(fmt >= 0 && fmt <= 3);

    size_t data_pos = 0;
    for (int i = 0; i != chunk_count; ++i)
    {
        IoBuffer header;

        size_t send_len = cur_message_length - (i * out_chunk_size_);
        if (send_len > out_chunk_size_)
        {
            send_len = out_chunk_size_;
        }

        if (i == 0)
        {
            header.WriteU8(fmt << 6 | cs_id);

            if (fmt == 0)
            {
                header.WriteU24(cur_timestamp_delta);
                header.WriteU24(cur_message_length);
                header.WriteU8(cur_message_type_id);
                header.WriteU32(htobe32(cur_message_stream_id));
            }
            else if (fmt == 1)
            {
                header.WriteU24(cur_timestamp_delta);
                header.WriteU24(cur_message_length);
                header.WriteU8(cur_message_type_id);
            }
            else if (fmt == 2)
            {
                header.WriteU24(cur_timestamp_delta);
            }
            else if (fmt == 3)
            {
            }

            if (payload.IsVideo())
            {
				if (payload.IsIFrame())
    			{   
    			    std::cout << LMSG << "I frame" << std::endl;
    			    header.WriteU8(0x17);
    			}   
    			else
    			{   
    			    header.WriteU8(0x27);
    			}   

    			header.WriteU8(0x01); // AVC nalu

    			uint32_t compositio_time_offset = payload.GetPts32() - payload.GetDts32();

    			header.WriteU24(compositio_time_offset);
            }
        }
        else
        {
            fmt = 3;
            header.WriteU8(fmt << 6 | cs_id);
        }

        uint8_t* buf = NULL;
        int size = 0;

        size = header.Read(buf, header.Size());
        socket_->Send(buf, size);
        if (i == 0 && payload.IsVideo())
        {
            socket_->Send(cur_info.msg + data_pos, send_len - 5);
            data_pos += send_len - 5;
        }
        else
        {
            socket_->Send(cur_info.msg + data_pos, send_len);
            data_pos += send_len;
        }
    }

    csid_pre_info_[cs_id] = cur_info;

    // FIXME
    return 0;
}

int RtmpProtocol::SendMediaData(const Payload& payload)
{
    RtmpMessage rtmp_message;

    rtmp_message.cs_id = 6;

    if (payload.IsAudio())
    {
        rtmp_message.cs_id = 4;
    }

    rtmp_message.timestamp = payload.GetDts();

    if (csid_pre_info_.count(rtmp_message.cs_id) == 0)
    {
        rtmp_message.timestamp_delta = 0;
    }
    else
    {
        rtmp_message.timestamp_delta = rtmp_message.timestamp - csid_pre_info_[rtmp_message.cs_id].timestamp;
    }

    rtmp_message.message_length = payload.GetAllLen();

    if (payload.IsAudio())
    {
        rtmp_message.message_type_id = kAudio;
    }
    else if (payload.IsVideo())
    {
        rtmp_message.message_type_id = kVideo;
        rtmp_message.message_length += 5;
    }

    rtmp_message.msg = payload.GetAllData();
    rtmp_message.len = payload.GetAllLen();

    return SendData(rtmp_message, payload);
}

int RtmpProtocol::SendVideoHeader(const std::string& header)
{
    std::string video_header;

    video_header.append(1, 0x17);
    video_header.append(1, 0x00);
    video_header.append(1, 0x00);
    video_header.append(1, 0x00);
    video_header.append(1, 0x00);
    video_header.append(header);

    SendRtmpMessage(6, 1, kVideo, (const uint8_t*)video_header.data(), video_header.size());

    return 0;
}

int RtmpProtocol::SendAudioHeader(const std::string& header)
{
	std::string audio_header;
    audio_header.append(1, 0xAF);
    audio_header.append(1, 0x00);
    audio_header.append(header);

    SendRtmpMessage(4, 1, kAudio, (const uint8_t*)audio_header.data(), audio_header.size());

    return 0;
}

int RtmpProtocol::SendMetaData(const std::string& metadata)
{
    SendRtmpMessage(4, 1, kMetaData_AMF0, (const uint8_t*)metadata.data(), metadata.size());

    return 0;
}

int RtmpProtocol::SendHandShakeStatus0()
{
    std::cout << LMSG << std::endl;

    uint8_t version = 3;

    socket_->Send(&version, 1);

    handshake_status_ = kStatus_1;

    return kSuccess;
}

int RtmpProtocol::SendHandShakeStatus1()
{
    std::cout << LMSG << std::endl;

    IoBuffer io_buffer;

    uint32_t time = Util::GetNowMs();
    uint32_t zero = 0;

    uint8_t buf[1528];

    io_buffer.WriteU32(time);
    io_buffer.WriteU32(zero);
    io_buffer.Write(buf, sizeof(buf));

    uint8_t* data = NULL;

    io_buffer.Read(data, s1_len);

    socket_->Send(data, s1_len);

    handshake_status_ = kStatus_2;

    return kSuccess;
}

int RtmpProtocol::SetOutChunkSize(const uint32_t& chunk_size)
{
    IoBuffer io_buffer;

    out_chunk_size_ = chunk_size;

    io_buffer.WriteU32(out_chunk_size_);

    uint8_t* data = NULL;

    io_buffer.Read(data, 4);

    SendRtmpMessage(2, 0, kSetChunkSize, data, 4);

    return kSuccess;
}

int RtmpProtocol::SetWindowAcknowledgementSize(const uint32_t& ack_window_size)
{
    IoBuffer io_buffer;

    io_buffer.WriteU32(ack_window_size);

    uint8_t* data = NULL;

    io_buffer.Read(data, 4);

    SendRtmpMessage(2, 0, kWindowAcknowledgementSize, data, 4);

    return kSuccess;
}

int RtmpProtocol::SetPeerBandwidth(const uint32_t& ack_window_size, const uint8_t& limit_type)
{
    IoBuffer io_buffer;

    io_buffer.WriteU32(ack_window_size);
    io_buffer.WriteU8(limit_type);

    uint8_t* data = NULL;

    io_buffer.Read(data, 5);

    SendRtmpMessage(2, 0, kSetPeerBandwidth, data, 5);

    return kSuccess;
}

int RtmpProtocol::SendUserControlMessage(const uint16_t& event, const uint32_t& data)
{
    IoBuffer io_buffer;

    io_buffer.WriteU16(event);
    io_buffer.WriteU32(data);

    uint8_t* buf = NULL;

    io_buffer.Read(buf, 6);

    SendRtmpMessage(2, 0, kUserControlMessage, buf, 6);

    return kSuccess;
}

int RtmpProtocol::SendConnect(const std::string& url)
{
    std::cout << LMSG << "url:" << url << std::endl;
    RtmpUrl rtmp_url;
    ParseRtmpUrl(url, rtmp_url);

    stream_ = rtmp_url.stream;

    any::String command_name("connect");
    any::Double transaction_id(GetTransactionId());

    any::String app(rtmp_url.app);
    //String tc_url("rtmp://" + rtmp_url.ip + ":" + Util::Num2Str(rtmp_url.port) + "/" + rtmp_url.app);
    any::String tc_url("rtmp://" + rtmp_url.ip + "/" + rtmp_url.app);
    
    std::map<std::string, any::Any*> m = {{"app", &app}, {"tcUrl", &tc_url}};

    std::map<std::string, any::Any*> empty;

    any::Map command_object(m);
    any::Map optional_uer_args(empty);

    std::vector<any::Any*> connect = {(any::Any*)&command_name, (any::Any*)&transaction_id, (any::Any*)&command_object, (any::Any*)&optional_uer_args};

    IoBuffer output;

    int ret = Amf0::Encode(connect, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "connect";

            id_command_[transaction_id_] = last_send_command_;

            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendCreateStream()
{
    any::String command_name("createStream");
    any::Double transaction_id(GetTransactionId());
    any::Null null;

    std::vector<any::Any*> create_stream = {&command_name, &transaction_id, &null};

    IoBuffer output;

    int ret = Amf0::Encode(create_stream, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "createStream";
            id_command_[transaction_id_] = last_send_command_;
            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendReleaseStream()
{
    any::String command_name("releaseStream");
    any::Double transaction_id(GetTransactionId());
    any::Null null;
    any::String playpath(stream_);

    std::vector<any::Any*> releaseStream = {&command_name, &transaction_id, &null, &playpath};

    IoBuffer output;

    int ret = Amf0::Encode(releaseStream, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "releaseStream";
            id_command_[transaction_id_] = last_send_command_;
            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendFCPublish()
{
    any::String command_name("FCPublish");
    any::Double transaction_id(GetTransactionId());
    any::Null null;
    any::String playpath("");

    std::vector<any::Any*> fcpublish = {&command_name, &transaction_id, &null, &playpath};

    IoBuffer output;

    int ret = Amf0::Encode(fcpublish, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(8, 1, kAmf0Command, data, len);

            last_send_command_ = "FCPublish";
            id_command_[transaction_id_] = last_send_command_;
            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendCheckBw()
{
    any::String command_name("_checkbw");
    any::Double transaction_id(GetTransactionId());
    any::Null null;

    std::vector<any::Any*> checkbw = {&command_name, &transaction_id, &null};

    IoBuffer output;

    int ret = Amf0::Encode(checkbw, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(3, 0, kAmf0Command, data, len);

            last_send_command_ = "_checkbw";
            id_command_[transaction_id_] = last_send_command_;
            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendPublish(const double& stream_id)
{
    any::String command_name("publish");
    any::Double transaction_id(GetTransactionId());
    any::Null null;
    any::String stream(stream_);
    any::String publish_type("live");

    std::vector<any::Any*> publish = {&command_name, &transaction_id, &null, &stream, &publish_type};

    IoBuffer output;

    int ret = Amf0::Encode(publish, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(8, stream_id, kAmf0Command, data, len);
            last_send_command_ = "publish";
            id_command_[transaction_id_] = last_send_command_;
            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendPlay(const double& stream_id)
{
    any::String command_name("play");
    any::Double transaction_id(GetTransactionId());
    any::Null null;
    any::String stream(stream_);
    any::Double start(-2);
    any::Double duration(-1);

    std::vector<any::Any*> publish = {&command_name, &transaction_id, &null, &stream, &start, &duration};

    IoBuffer output;

    int ret = Amf0::Encode(publish, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;

    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(8, stream_id, kAmf0Command, data, len);
            last_send_command_ = "play";
            id_command_[transaction_id_] = last_send_command_;
            std::cout << LMSG << "send [" << last_send_command_ << " command]" << std::endl;
        }
    }

    return kSuccess;
}

int RtmpProtocol::SendAudio(const RtmpMessage& audio)
{
    UNUSED(audio);

    return kSuccess;
}

int RtmpProtocol::SendVideo(const RtmpMessage& video)
{
    UNUSED(video);

    return kSuccess;
}

int RtmpProtocol::HandleConnected(Fd& socket)
{
    UNUSED(socket);

    GetTcpSocket()->SetConnected();
    GetTcpSocket()->EnableRead();
    GetTcpSocket()->DisableWrite();

    if (role_ == RtmpRole::kPushServer || role_ == RtmpRole::kPullServer)
    {
        if (handshake_status_ == kStatus_0)
        {
            SendHandShakeStatus0();
            SendHandShakeStatus1();
        }
    }

    return kSuccess;
}

int RtmpProtocol::OnPendingArrive()
{
    std::cout << LMSG << std::endl;

    MediaPublisher* media_publisher = g_local_stream_center.GetMediaPublisherByAppStream(app_, stream_);

    if (media_publisher == NULL)
    {
        std::cout << LMSG << "no found app:" << app_ << ", stream_:" << stream_ << std::endl;
        return kClose;
    }

    any::String on_status("onStatus");
    any::Double transaction_id(0.0);
    any::Null null;

    any::String code("NetStream.Play.Start");
    any::Map information({{"code", (any::Any*)&code}});

    IoBuffer output;
    std::vector<any::Any*> play_result = {(any::Any*)&on_status, (any::Any*)&transaction_id, (any::Any*)&null, (any::Any*)&information};
    int ret = Amf0::Encode(play_result, output);
    std::cout << LMSG << "Amf0 encode ret:" << ret << std::endl;
    if (ret == 0)
    {
        uint8_t* data = NULL;
        int len = output.Read(data, output.Size());

        if (data != NULL && len > 0)
        {
            SendRtmpMessage(pending_rtmp_msg_.cs_id, pending_rtmp_msg_.message_stream_id, kAmf0Command, data, len);
        }

        SetPublisher(media_publisher);
        media_publisher->AddSubscriber(this);
    }

    return kSuccess;
}
