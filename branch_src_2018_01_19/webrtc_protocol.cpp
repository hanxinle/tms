#include <iostream>
#include <map>

#include "bit_stream.h"
#include "common_define.h"
#include "crc32.h"
#include "global.h"
#include "io_buffer.h"
#include "socket_util.h"
#include "udp_socket.h"
#include "webrtc_protocol.h"

#include "rtp_header.h"

#include "openssl/srtp.h"

#include "webm/file_reader.h"

#include "webrtc/base/bytebuffer.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/p2p/base/stun.h"

using namespace cricket;
using namespace rtc;
using namespace socket_util;
using namespace std;
using namespace webrtc;
using namespace webm;

const int SRTP_MASTER_KEY_KEY_LEN = 16;
const int SRTP_MASTER_KEY_SALT_LEN = 14;

static int HmacEncode(const string& algo, const char* key, const int& key_length,  
                	  const char* input, const int& input_length,  
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
        cout << LMSG << "Algorithm " << algo << " is not supported by this program!" << endl;  
        return -1;  
    }  
  
    HMAC_CTX ctx;  
    HMAC_CTX_init(&ctx);  
    HMAC_Init_ex(&ctx, key, strlen(key), engine, NULL);  
    HMAC_Update(&ctx, (const unsigned char*)input, input_length);
  
    HMAC_Final(&ctx, output, &output_length);  
    HMAC_CTX_cleanup(&ctx);  
  
    return 0;  
}  

WebrtcProtocol::WebrtcProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket),
    dtls_hello_send_(false),
    dtls_(NULL),
    dtls_handshake_done_(false)
{
}

WebrtcProtocol::~WebrtcProtocol()
{
}

int WebrtcProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;
    int len = io_buffer.Read(data, io_buffer.Size());

    if (len > 0)
    {
        cout << LMSG << Util::Bin2Hex(data, len) << endl;

		if ((data[0] == 0) || (data[0] == 1)) 
   		{   
            // RFC 5389
            OnStun(data, len);
            cout << LMSG << "stun" << endl;
   		}   
   		else if ((data[0] >= 128) && (data[0] <= 191))
   		{   
            OnRtpRtcp(data, len);
            cout << LMSG << "rtp/rtcp" << endl;
   		}   
   		else if ((data[0] >= 20) && (data[0] <= 64))
   		{   
            OnDtls(data, len);
            cout << LMSG << "dtls" << endl;
   		}
    }

    return kSuccess;
}

int WebrtcProtocol::OnStun(const uint8_t* data, const size_t& len)
{
    BitBuffer bit_buffer(data, len);

    uint16_t stun_message_type = 0;
    bit_buffer.GetBytes(2, stun_message_type);

    uint16_t message_length = 0;
    bit_buffer.GetBytes(2, message_length);

    if (! bit_buffer.MoreThanBytes(4 + 12))
    {
        return kError;
    }

    string magic_cookie = "";
    bit_buffer.GetString(4, magic_cookie);

    string transcation_id = "";
    bit_buffer.GetString(12, transcation_id);

	//0x0001  :  Binding Request
    //0x0101  :  Binding Response
    //0x0111  :  Binding Error Response
    //0x0002  :  Shared Secret Request
    //0x0102  :  Shared Secret Response
    //0x0112  :  Shared Secret Error Response
    cout << LMSG
         << "len:" << len
         <<",stun_message_type:" << stun_message_type
         << ",message_length:" << message_length
         << ",transcation_id:" << Util::Bin2Hex(transcation_id)
         << endl;


    cout << LMSG << TRACE << endl;

    string username = "";

    while (true)
    {
        if (! bit_buffer.MoreThanBytes(4))
        {
            cout << LMSG << endl;
            break;
        }

        uint16_t type = 0;
        uint16_t length = 0;

        bit_buffer.GetBytes(2, type);
        bit_buffer.GetBytes(2, length);


        cout << LMSG << "type:" << type << ",length:" << length << endl;

        if (! bit_buffer.MoreThanBytes(length))
        {
            cout << LMSG << endl;
            break;
        }

        string value;
        bit_buffer.GetString(length, value);

		//0x0001: MAPPED-ADDRESS
   		//0x0002: RESPONSE-ADDRESS
   		//0x0003: CHANGE-REQUEST
   		//0x0004: SOURCE-ADDRESS
   		//0x0005: CHANGED-ADDRESS
   		//0x0006: USERNAME
   		//0x0007: PASSWORD
   		//0x0008: MESSAGE-INTEGRITY
   		//0x0009: ERROR-CODE
   		//0x000a: UNKNOWN-ATTRIBUTES
   		//0x000b: REFLECTED-FROM

		switch (type)
        {
            case 0x0001: 
            {
                cout << LMSG << "MAPPED-ADDRESS" << endl;
            } 
            break;

            case 0x0002: 
            {
                cout << LMSG << "RESPONSE-ADDRESS" << endl;
            } 
            break;

            case 0x0003: 
            {
                cout << LMSG << "CHANGE-ADDRESS" << endl;
            } 
            break;

            case 0x0004: 
            {
                cout << LMSG << "SOURCE-ADDRESS" << endl;
            } 
            break;

            case 0x0005: 
            {
                cout << LMSG << "CHANGED-ADDRESS" << endl;
            } 
            break;

            case 0x0006: 
            {
                cout << LMSG << "USERNAME" << endl;
                cout << LMSG << value << endl;
                username = value;
            } 
            break;

            case 0x0007: 
            {
                cout << LMSG << "PASSWORD" << endl;
            } 
            break;

            case 0x0008: 
            {
                cout << LMSG << "MESSAGE-INTEGRITY" << endl;
            } 
            break;

            case 0x0009: 
            {
                cout << LMSG << "ERROR-CODE" << endl;
            }
            break;

            case 0x000a: 
            {
                cout << LMSG << "UNKNOWN-ATTRIBUTES" << endl;
            } 
            break;

            case 0x000b: 
            {
                cout << LMSG << "REFLECTED-FROM" << endl;
            }
            break;

            case 0x0014:
            {
                cout << LMSG << "REALM" << endl;
            }
            break;

            case 0x0015:
            {
                cout << LMSG << "NONCE" << endl;
            }
            break;

            case 0x0020:
            {
                cout << LMSG << "XOR-MAPPED-ADDRESS" << endl;
            };
            break;

            case 0x8022:
            {
                cout << LMSG << "SOFTWARE" << endl;
            };
            break;

            case 0x8023:
            {
                cout << LMSG << "ALTERNATE-SERVER" << endl;
            };
            break;

            case 0x8028:
            {
                cout << LMSG << "FINGERPRINT" << endl;
            };
            break;

            default : 
            {
                cout << LMSG << "Undefine" << endl;
            } 
            break;
        }
    }

    switch (stun_message_type)
    {
        case 0x0001:
        {
#if 1
            cout << LMSG << "Binding Request" << endl;

            uint32_t magic_cookie = 0x2112A442;

            BitStream binding_response;

            binding_response.WriteBytes(2, 0x0020);
            binding_response.WriteBytes(2, 8);
            binding_response.WriteBytes(1, 0x00);
            binding_response.WriteBytes(1, 0x01); // IPv4
            binding_response.WriteBytes(2, (GetUdpSocket()->GetClientPort() ^ (magic_cookie >> 16)));

            uint32_t ip_num;
            IpStr2Num(GetUdpSocket()->GetClientIp(), ip_num);
            binding_response.WriteBytes(4, htobe32(htobe32(magic_cookie) ^ ip_num));

            binding_response.WriteBytes(2, 0x0006); // USERNAME
            binding_response.WriteBytes(2, username.size());
            binding_response.WriteData(username.size(), (const uint8_t*)username.data());

            uint8_t hmac[20] = {0};
            {
                BitStream hmac_input;
                hmac_input.WriteBytes(2, 0x0101); // Binding Response
                hmac_input.WriteBytes(2, binding_response.SizeInBytes() + 4 + 20);
                hmac_input.WriteBytes(4, magic_cookie);
                hmac_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
                hmac_input.WriteData(binding_response.SizeInBytes(), binding_response.GetData());
                unsigned int out_len = 0;
                HmacEncode("sha1", g_local_ice_pwd.data(), g_local_ice_pwd.size(), (const char*)hmac_input.GetData(), hmac_input.SizeInBytes(), hmac, out_len);

                cout << LMSG << "g_local_ice_pwd:" << g_local_ice_pwd << endl;
                cout << LMSG << "hamc out_len:" << out_len << endl;
            }

            binding_response.WriteBytes(2, 0x0008);
            binding_response.WriteBytes(2, 20);
            binding_response.WriteData(20, hmac);

            uint32_t crc_32 = 0;
            {
                BitStream crc32_input;
                crc32_input.WriteBytes(2, 0x0101); // Binding Response
                crc32_input.WriteBytes(2, binding_response.SizeInBytes() + 8);
                crc32_input.WriteBytes(4, magic_cookie);
                crc32_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
                crc32_input.WriteData(binding_response.SizeInBytes(), binding_response.GetData());
                CRC32 crc32;
                cout << LMSG << "my crc32 input:" << Util::Bin2Hex(crc32_input.GetData(), crc32_input.SizeInBytes()) << endl;
                crc_32 = crc32.GetCrc32(crc32_input.GetData(), crc32_input.SizeInBytes());
                cout << LMSG << "crc32:" << crc_32 << endl;
                crc_32 = crc_32 ^ 0x5354554E;
                cout << LMSG << "crc32:" << crc_32 << endl;
            }

            binding_response.WriteBytes(2, 0x8028);
            binding_response.WriteBytes(2, 4);
            binding_response.WriteBytes(4, crc_32);

            BitStream binding_response_header;
            binding_response_header.WriteBytes(2, 0x0101); // Binding Response
            binding_response_header.WriteBytes(2, binding_response.SizeInBytes());
            binding_response_header.WriteBytes(4, magic_cookie);
            binding_response_header.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
            binding_response_header.WriteData(binding_response.SizeInBytes(), binding_response.GetData());

            cout << LMSG << "myself binding_response\n" 
                 << Util::Bin2Hex(binding_response_header.GetData(), binding_response_header.SizeInBytes()) << endl;

            GetUdpSocket()->Send(binding_response_header.GetData(), binding_response_header.SizeInBytes());

            // send binding request

            if (true)
            {
                BitStream binding_request;
                string username = g_remote_ice_ufrag + ":" + g_local_ice_ufrag;
                binding_request.WriteBytes(2, 0x0006); // USERNAME
                binding_request.WriteBytes(2, username.size());
                binding_request.WriteData(username.size(), (const uint8_t*)username.data());

                uint8_t hmac[20] = {0};
                {
                    BitStream hmac_input;
                    hmac_input.WriteBytes(2, 0x0001); // Binding Request
                    hmac_input.WriteBytes(2, binding_request.SizeInBytes() + 4 + 20);
                    hmac_input.WriteBytes(4, magic_cookie);
                    hmac_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
                    hmac_input.WriteData(binding_request.SizeInBytes(), binding_request.GetData());
                    unsigned int out_len = 0;
                    HmacEncode("sha1", g_remote_ice_pwd.data(), g_remote_ice_pwd.size(), (const char*)hmac_input.GetData(), hmac_input.SizeInBytes(), hmac, out_len);

                    cout << LMSG << "g_remote_ice_pwd:" << g_remote_ice_pwd << endl;
                    cout << LMSG << "hamc out_len:" << out_len << endl;
                }

                binding_request.WriteBytes(2, 0x0008);
                binding_request.WriteBytes(2, 20);
                binding_request.WriteData(20, hmac);

                uint32_t crc_32 = 0;
                {
                    BitStream crc32_input;
                    crc32_input.WriteBytes(2, 0x0001); // Binding Response
                    crc32_input.WriteBytes(2, binding_request.SizeInBytes() + 8);
                    crc32_input.WriteBytes(4, magic_cookie);
                    crc32_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
                    crc32_input.WriteData(binding_request.SizeInBytes(), binding_request.GetData());
                    CRC32 crc32;
                    cout << LMSG << "my crc32 input:" << Util::Bin2Hex(crc32_input.GetData(), crc32_input.SizeInBytes()) << endl;
                    crc_32 = crc32.GetCrc32(crc32_input.GetData(), crc32_input.SizeInBytes());
                    cout << LMSG << "crc32:" << crc_32 << endl;
                    crc_32 = crc_32 ^ 0x5354554E;
                    cout << LMSG << "crc32:" << crc_32 << endl;
                }

                binding_request.WriteBytes(2, 0x8028);
                binding_request.WriteBytes(2, 4);
                binding_request.WriteBytes(4, crc_32);

                BitStream binding_request_header;
                binding_request_header.WriteBytes(2, 0x0001); // Binding Request
                binding_request_header.WriteBytes(2, binding_request.SizeInBytes());
                binding_request_header.WriteBytes(4, magic_cookie);
                binding_request_header.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
                binding_request_header.WriteData(binding_request.SizeInBytes(), binding_request.GetData());

                cout << LMSG << "myself send binding_request\n" 
                     << Util::Bin2Hex(binding_request_header.GetData(), binding_request_header.SizeInBytes()) << endl;


                GetUdpSocket()->Send(binding_request_header.GetData(), binding_request_header.SizeInBytes());

            }
#else
			ByteBufferReader byte_buffer_read((const char*)data, len);
    		StunMessage stun_req;

    		if (! stun_req.Read(&byte_buffer_read))
    		{   
    		    cout << LMSG << "invalid stun message" << endl;
    		}   

    		if (stun_req.type() == STUN_BINDING_REQUEST)
    		{ 
                const StunByteStringAttribute* username_attr = stun_req.GetByteString(STUN_ATTR_USERNAME);

				IceMessage stun_rsp;

            	stun_rsp.SetType(GetStunSuccessResponseType(stun_req.type()));
            	stun_rsp.SetTransactionID(stun_req.transaction_id());

                uint32_t ip_num;
                IpStr2Num(GetUdpSocket()->GetClientIp(), ip_num);

            	SocketAddress addr(htobe32(ip_num), GetUdpSocket()->GetClientPort());
            	stun_rsp.AddAttribute(new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, addr));
            	stun_rsp.AddAttribute(new StunByteStringAttribute(STUN_ATTR_USERNAME, username_attr->GetString()));
            	stun_rsp.AddMessageIntegrity(g_local_ice_pwd);
            	stun_rsp.AddFingerprint();

            	ByteBufferWriter byte_buffer_write;
            	stun_rsp.Write(&byte_buffer_write);

                cout << LMSG << "webrtc binding_response\n" << Util::Bin2Hex((const uint8_t*)byte_buffer_write.Data(), byte_buffer_write.Length()) << endl;

            	GetUdpSocket()->Send((const uint8_t*)byte_buffer_write.Data(), byte_buffer_write.Length());
            }
#endif

            if (! dtls_hello_send_)
            {
                cout << LMSG << "dtls send clienthello" << endl;

                dtls_hello_send_ = true;

#ifdef USE_VP8_WEBM
                media_input_.Open("input_vp8.webm");
#else
                media_input_.Open("input_vp9.webm");
#endif

                if (dtls_ == NULL)
                {
                    dtls_ = SSL_new(g_dtls_ctx);

					SSL_set_connect_state(dtls_);

        			bio_in_  = BIO_new(BIO_s_mem());
        			bio_out_ = BIO_new(BIO_s_mem());

        			SSL_set_bio(dtls_, bio_in_, bio_out_);

                    Handshake();
                }
            }

        }
        break;

        case 0x0101:
        {
        }
        break;

        case 0x0111:
        {
        }
        break;

        case 0x0002:
        {
        }
        break;

        case 0x0102:
        {
        }
        break;

        case 0x0112:
        {
        }
        break;

        default:
        {
        }
        break;
    }

    cout << LMSG << TRACE << endl;



    return kSuccess;
}

int WebrtcProtocol::OnDtls(const uint8_t* data, const size_t& len)
{
    cout << LMSG <<endl;

    if (! dtls_handshake_done_)
    {
		BIO_reset(bio_in_);
        BIO_reset(bio_out_);

        BIO_write(bio_in_, data, len);

        Handshake();
    }

	return 0;
}

int WebrtcProtocol::OnRtpRtcp(const uint8_t* data, const size_t& len)
{
    if (len < 12)
    {
        return kNoEnoughData;
    }

    cout << LMSG << "Rtp Header Peek:" << Util::Bin2Hex(data, 12) << endl;

    uint8_t unprotect_buf[4096] = {0};
    int unprotect_buf_len = len;
    memcpy(unprotect_buf, data, len);

    BitBuffer bit_buffer(data, len);

    uint8_t version = 0;
    bit_buffer.GetBits(2, version);

    uint8_t padding = 0;
    bit_buffer.GetBits(1, padding);

    uint8_t extension = 0;
    bit_buffer.GetBits(1, extension);

    uint8_t csrc_count = 0;
    bit_buffer.GetBits(4, csrc_count);

    uint8_t marker = 0;
    bit_buffer.GetBits(1, marker);
    
    uint8_t payload_type = 0;
    bit_buffer.GetBits(7, payload_type);

    uint16_t sequence_number = 0;
    bit_buffer.GetBytes(2, sequence_number);

    uint32_t timestamp = 0;
    bit_buffer.GetBytes(4, timestamp);

    uint32_t ssrc = 0;
    bit_buffer.GetBytes(4, ssrc);

    for (uint8_t c = 0; c != csrc_count; ++c)
    {
        uint32_t csrc = 0;
        bit_buffer.GetBytes(4, csrc);
    }

    cout << LMSG << "[RTP Header] # version:" << (int)version
                 << ",padding:" << (int)padding
                 << ",extension:" << (int)extension
                 << ",csrc_count:" << (int)csrc_count
                 << ",marker:" << (int)marker
                 << ",payload_type:" << (int)payload_type
                 << ",sequence_number:" << sequence_number
                 << ",timestamp:" << timestamp
                 << ",ssrc:" << ssrc
                 << endl;

    int ret = srtp_unprotect(srtp_recv_, unprotect_buf, &unprotect_buf_len);
    if (ret == 0)
    {
        cout << LMSG << "srtp_unprotect success" << endl;
    }
    else
    {
        cout << LMSG << "srtp_unprotect ret:" << ret << endl;
    }

    cout << LMSG << "unprotect_buf_len:" << unprotect_buf_len << endl;

    return 0;
}

int WebrtcProtocol::Handshake()
{
	int ret = SSL_do_handshake(dtls_);

    unsigned char *out_bio_data;
    int out_bio_len = BIO_get_mem_data(bio_out_, &out_bio_data);

    int err = SSL_get_error(dtls_, ret); 
    switch(err)
    {   
        case SSL_ERROR_NONE:
        {   
            dtls_handshake_done_ = true;
            cout << LMSG << "handshake done" << endl;

			unsigned char material[SRTP_MASTER_KEY_LEN * 2] = {0};  // client(SRTP_MASTER_KEY_KEY_LEN + SRTP_MASTER_KEY_SALT_LEN) + server
    		char dtls_srtp_lable[] = "EXTRACTOR-dtls_srtp";
    		if (! SSL_export_keying_material(dtls_, material, sizeof(material), dtls_srtp_lable, strlen(dtls_srtp_lable), NULL, 0, 0)) 
    		{   
    		    cout << LMSG << "SSL_export_keying_material error" << endl;
    		}   
            else
            {
    		    size_t offset = 0;

    		    string sClientMasterKey(reinterpret_cast<char*>(material), SRTP_MASTER_KEY_KEY_LEN);
    		    offset += SRTP_MASTER_KEY_KEY_LEN;
    		    string sServerMasterKey(reinterpret_cast<char*>(material + offset), SRTP_MASTER_KEY_KEY_LEN);
    		    offset += SRTP_MASTER_KEY_KEY_LEN;
    		    string sClientMasterSalt(reinterpret_cast<char*>(material + offset), SRTP_MASTER_KEY_SALT_LEN);
    		    offset += SRTP_MASTER_KEY_SALT_LEN;
    		    string sServerMasterSalt(reinterpret_cast<char*>(material + offset), SRTP_MASTER_KEY_SALT_LEN);

    		    client_key_ = sClientMasterKey + sClientMasterSalt;
    		    server_key_ = sServerMasterKey + sServerMasterSalt;

                cout << LMSG << "client_key_:" << client_key_.size() << ",server_key_:" << server_key_.size() << endl;

                srtp_init();

                // rtp_send
                {
					// send
        			srtp_policy_t policy;
        			bzero(&policy, sizeof(policy));

        			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
        			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

        			policy.ssrc.type = ssrc_any_outbound;
    
        			policy.ssrc.value = 0;
        			policy.window_size = 8192; // seq 相差8192认为无效
        			policy.allow_repeat_tx = 1;
        			policy.next = NULL;

        			uint8_t *key = new uint8_t[client_key_.size()];
        			memcpy(key, client_key_.data(), client_key_.size());
        			policy.key = key;

        			int ret = srtp_create(&srtp_send_, &policy);
        			if (ret != 0)
        			{   
        			    cout << LMSG << "srtp_create error:" << ret << endl;
        			}   
                    else
                    {
                        cout << LMSG << "srtp_send init success" << endl;
                    }

        			delete [] key;
                }

				// srtp_recv
                {
        			srtp_policy_t policy;
        			bzero(&policy, sizeof(policy));

        			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
        			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

        			policy.ssrc.type = ssrc_any_inbound;

        			policy.ssrc.value = 0;
        			policy.window_size = 8192; // seq 相差8192认为无效
        			policy.allow_repeat_tx = 1;
        			policy.next = NULL;

        			uint8_t *key = new uint8_t[server_key_.size()];
        			memcpy(key, server_key_.data(), server_key_.size());
        			policy.key = key;

        			int ret = srtp_create(&srtp_recv_, &policy);
        			if (ret != 0)
        			{
        			    cout << LMSG << "srtp_create error:" << ret << endl;
        			}
                    else
                    {
                        cout << LMSG << "srtp_recv init success" << endl;
                    }

        			delete [] key;
                }
            }
        }  
        break;

        case SSL_ERROR_WANT_READ:
        {   
            cout << LMSG << "handshake want read" << endl;
        }   
        break;

        case SSL_ERROR_WANT_WRITE:
        {   
            cout << LMSG << "handshake want write" << endl;
        }
        break;

        default:
        {   
            cout << LMSG << endl;
        }   
        break;
    }   

    if (out_bio_len)
    {   
        cout << LMSG << "send handshake msg, len:" << out_bio_len << endl;
        GetUdpSocket()->Send(out_bio_data, out_bio_len);
    }

    return 0;
}

int WebrtcProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return 0;
}

int WebrtcProtocol::EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    cout << LMSG << "count:" << count << endl;

    if (dtls_handshake_done_)
    {
        for (int i = 0; i != 5; ++i)
        {
            uint8_t* frame_data = NULL;
            int frame_len = 0;
            int flag = 0;
            bool is_video = false;
            uint64_t timestamp = 0;

            int ret = media_input_.ReadFrame(frame_data, frame_len, flag, is_video, timestamp);

            if (ret != 0)
            {
                return 0;
            }

            cout << LMSG << "is_video:" << is_video << ",timestamp:" << timestamp << endl;

            if (is_video)
            {
                cout << LMSG << "send vp8 message" << endl;

                static int picture_id_ = 0;

		        RTPVideoTypeHeader rtp_video_head;

#ifdef USE_VP8_WEBM
                RTPVideoHeaderVP8& rtp_header_vp8 = rtp_video_head.VP8;
                rtp_header_vp8.InitRTPVideoHeaderVP8();
                rtp_header_vp8.pictureId = ++picture_id_;
                rtp_header_vp8.nonReference = 0;

                webrtc::FrameType frame_type = kVideoFrameDelta;
                if (flag & AV_PKT_FLAG_KEY)
                {
                    frame_type = kVideoFrameKey;
                    rtp_header_vp8.nonReference = 1;
                }

                // 编码后的视频帧打包为RTP
                RtpPacketizer* rtp_packetizer = RtpPacketizer::Create(kRtpVideoVp8, 1200, &rtp_video_head, frame_type);
#else
                RTPVideoHeaderVP9& rtp_header_vp9 = rtp_video_head.VP9;
                rtp_header_vp9.InitRTPVideoHeaderVP9();
                rtp_header_vp9.picture_id = ++picture_id_;
                rtp_header_vp9.inter_pic_predicted = 1;

                webrtc::FrameType frame_type = kVideoFrameDelta;
                if (flag & AV_PKT_FLAG_KEY)
                {
                    frame_type = kVideoFrameKey;
                    rtp_header_vp9.inter_pic_predicted = 0;
                }

                // 编码后的视频帧打包为RTP
                RtpPacketizer* rtp_packetizer = RtpPacketizer::Create(kRtpVideoVp9, 1200, &rtp_video_head, frame_type);
#endif

                rtp_packetizer->SetPayloadData(frame_data, frame_len, NULL);

                bool last_packet = false;
                do  
                {   
                    uint8_t rtp[1500] = {0};
                    uint8_t* rtp_packet = rtp + 12; 
                    size_t rtp_packet_len = 0;
                    if (! rtp_packetizer->NextPacket(rtp_packet, &rtp_packet_len, &last_packet))
                    {   
                        cerr << LMSG << "packet rtp error" << endl;
                    }   
                    else
                    {   
                        RtpHeader rtp_header;

                        rtp_header.setSSRC(3233846889);
                        rtp_header.setMarker(last_packet ? 1 : 0); 
                        static uint32_t video_seq_ = 0;
                        rtp_header.setSeqNumber(++video_seq_);
#ifdef USE_VP8_WEBM
                        rtp_header.setPayloadType(96);
#else
                        rtp_header.setPayloadType(98);
#endif
                        rtp_header.setTimestamp((uint32_t)timestamp * 90);

                        memcpy(rtp, &rtp_header, rtp_header.getHeaderLength()/*rtp head size*/);

                        char protect_buf[1500];
                        int protect_buf_len = rtp_header.getHeaderLength() + rtp_packet_len;
                        memcpy(protect_buf, rtp, protect_buf_len);

                        int ret = srtp_protect(srtp_send_, protect_buf, &protect_buf_len);
                        if (ret == 0)
                        {
                            cout << LMSG << "srtp_protect success" << endl;
                            GetUdpSocket()->Send((const uint8_t*)protect_buf, protect_buf_len);
                        }
                        else
                        {
                            cout << LMSG << "srtp_protect faile:" << ret << endl;
                        }

                        cout << LMSG << "srtp protect_buf_len:" << protect_buf_len << endl;

                        //cout << LMSG << "==> packet rtp success <==" << endl;
                    }   
                }   
                while (! last_packet);

		        delete rtp_packetizer;
            }
            else
            {
                cout << LMSG << "send opus message" << endl;

				RtpHeader rtp_header;

            	uint8_t rtp[1500];
            	uint8_t* rtp_packet = rtp + 12; 

                static uint32_t audio_seq_ = 0;

            	rtp_header.setSSRC(3233846889+1);
            	rtp_header.setSeqNumber(++audio_seq_);
            	rtp_header.setPayloadType(111);
            	rtp_header.setTimestamp((uint32_t)timestamp * 48);

            	memcpy(rtp, &rtp_header, 12/*rtp head size*/);
            	memcpy(rtp_packet, frame_data, frame_len);

                char protect_buf[1500];
                int protect_buf_len = 12 + frame_len;;
                memcpy(protect_buf, rtp, protect_buf_len);

                int ret = srtp_protect(srtp_send_, protect_buf, &protect_buf_len);
                if (ret == 0)
                {
                    cout << LMSG << "srtp_protect success" << endl;
                    GetUdpSocket()->Send((const uint8_t*)protect_buf, protect_buf_len);
                }
                else
                {
                    cout << LMSG << "srtp_protect faile:" << ret << endl;
                }

                cout << LMSG << "srtp protect_buf_len:" << protect_buf_len << endl;
            }
        }
    }

    return kSuccess;
}
