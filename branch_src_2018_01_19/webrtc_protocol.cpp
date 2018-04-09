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

#include "webrtc/base/bytebuffer.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/p2p/base/stun.h"

using namespace cricket;
using namespace rtc;
using namespace socket_util;
using namespace std;
using namespace webrtc;

const int SRTP_MASTER_KEY_KEY_LEN = 16;
const int SRTP_MASTER_KEY_SALT_LEN = 14;

extern WebrtcProtocol* g_debug_webrtc;

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


static uint32_t get_host_priority(uint16_t local_pref, bool is_rtp)
{   
    uint32_t pref = 126;
    return (pref << 24) + (local_pref << 8) + ((256 - (is_rtp ? 1 : 2)) << 0); 
}

WebrtcProtocol::WebrtcProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket),
    dtls_hello_send_(false),
    dtls_(NULL),
    dtls_handshake_done_(false),
    media_input_(NULL),
    timestamp_base_(0),
    timestamp_(0),
    media_input_open_count_(0),
    media_input_read_video_frame_count(0),
    send_begin_time_(Util::GetNowMs()),
    initiate_tag_(0)
{
}

WebrtcProtocol::~WebrtcProtocol()
{
    close(socket_->GetFd());
}

int WebrtcProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;
    int len = io_buffer.Read(data, io_buffer.Size());

    if (len > 0)
    {
        cout << LMSG << "webrtc recv\n" << Util::Bin2Hex(data, len) << endl;

		if ((data[0] == 0) || (data[0] == 1)) 
   		{   
            // RFC 5389
            OnStun(data, len);
            cout << LMSG << (long)this << ", stun" << endl;
   		}   
   		else if ((data[0] >= 128) && (data[0] <= 191))
   		{   
            OnRtpRtcp(data, len);
            cout << LMSG << (long)this << ", rtp/rtcp" << endl;
   		}   
   		else if ((data[0] >= 20) && (data[0] <= 64))
   		{   
            OnDtls(data, len);
            cout << LMSG << (long)this << ", dtls" << endl;
   		}
        else
        {
            cout << LMSG << (long)this <<", unknown" <<endl;
        }
    }

    return kSuccess;
}

void WebrtcProtocol::SendVideoData(const uint8_t* data, const int& size, const uint32_t& timestamp, const int& flag)
{
    cout << LMSG << "send vp8 message, size:" << size << endl;

    static int picture_id_ = 0;

	RTPVideoTypeHeader rtp_video_head;

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
    rtp_packetizer->SetPayloadData(data, size, NULL);

    bool last_packet = false;
    do  
    {   
        uint8_t rtp[1500] = {0};
        uint8_t* rtp_packet = rtp + 12; 
        size_t rtp_packet_len = 0;
        if (! rtp_packetizer->NextPacket(rtp_packet, &rtp_packet_len, &last_packet))
        {   
            cout << LMSG << "packet rtp error" << endl;
        }   
        else
        {   
            RtpHeader rtp_header;

            rtp_header.setSSRC(3233846889);
            rtp_header.setMarker(last_packet ? 1 : 0); 
            static uint32_t video_seq_ = 0;
            rtp_header.setSeqNumber(++video_seq_);
            rtp_header.setPayloadType(96);
            rtp_header.setTimestamp((uint32_t)(timestamp) * 90);

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

void WebrtcProtocol::SendAudioData(const uint8_t* data, const int& size, const uint32_t& timestamp, const int& flag)
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
    memcpy(rtp_packet, data, size);

    char protect_buf[1500];
    int protect_buf_len = 12 + size;;
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

int WebrtcProtocol::ProtectRtp(const uint8_t* un_protect_rtp, const int& un_protect_rtp_len, uint8_t* protect_rtp, int& protect_rtp_len)
{
    memcpy(protect_rtp, un_protect_rtp, un_protect_rtp_len);

    int ret = srtp_protect(srtp_send_, protect_rtp, &protect_rtp_len);

    return ret;
}

int WebrtcProtocol::UnProtectRtp(const uint8_t* protect_rtp, const int& protect_rtp_len, uint8_t* un_protect_rtp, int& un_protect_rtp_len)
{
    memcpy(un_protect_rtp, protect_rtp, protect_rtp_len);

    int ret = srtp_unprotect(srtp_recv_, un_protect_rtp, &un_protect_rtp_len);
    
    return ret;
}

int WebrtcProtocol::ProtectRtcp(const uint8_t* un_protect_rtcp, const int& un_protect_rtcp_len, uint8_t* protect_rtcp, int& protect_rtcp_len)
{
    memcpy(protect_rtcp, un_protect_rtcp, un_protect_rtcp_len);

    int ret = srtp_unprotect_rtcp(srtp_send_, protect_rtcp, &protect_rtcp_len);

    return ret;
}

int WebrtcProtocol::UnProtectRtcp(const uint8_t* protect_rtcp, const int& protect_rtcp_len, uint8_t* un_protect_rtcp, int& un_protect_rtcp_len)
{
    memcpy(un_protect_rtcp, protect_rtcp, protect_rtcp_len);

    int ret = srtp_unprotect_rtcp(srtp_recv_, un_protect_rtcp, &un_protect_rtcp_len);
    
    return ret;
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
    string local_ufrag = "";
    string remote_ufrag = "";

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

                auto pos = username.find(":");
                if (pos != string::npos)
                {
                    local_ufrag = username.substr(0, pos);
                    remote_ufrag = username.substr(pos + 1);

                    cout << LMSG << "local_ufrag:" << local_ufrag << ",remote_ufrag:" << remote_ufrag << endl;
                }
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

            case 0x0025:
            {
                cout << LMSG << "PRIORITY" << endl;
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

            case 0x8029:
            {
                cout << LMSG << "ICE_CONTROLLED" << endl;
            };
            break;

            case 0x802A:
            {
                cout << LMSG << "ICE_CONTROLLING" << endl;
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
                HmacEncode("sha1", local_pwd_.data(), local_pwd_.size(), (const char*)hmac_input.GetData(), hmac_input.SizeInBytes(), hmac, out_len);

                cout << LMSG << "local_pwd_:" << local_pwd_ << endl;
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
                CRC32 crc32(CRC32_STUN);
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
            	stun_rsp.AddMessageIntegrity(local_pwd_);
            	stun_rsp.AddFingerprint();

            	ByteBufferWriter byte_buffer_write;
            	stun_rsp.Write(&byte_buffer_write);

                cout << LMSG << "webrtc binding_response\n" << Util::Bin2Hex((const uint8_t*)byte_buffer_write.Data(), byte_buffer_write.Length()) << endl;

            	GetUdpSocket()->Send((const uint8_t*)byte_buffer_write.Data(), byte_buffer_write.Length());
            }
#endif

            if (! g_webrtc_mgr->IsRemoteUfragExist(remote_ufrag))
            {
                cout << LMSG << "connect udp socket:" << GetUdpSocket()->GetClientIp() << ":" << GetUdpSocket()->GetClientPort() << endl;
                g_webrtc_mgr->AddRemoteUfrag(remote_ufrag);

                int fd = CreateNonBlockUdpSocket();
                ReuseAddr(fd);
                Bind(fd, "0.0.0.0", 11445);
                Connect(fd, GetUdpSocket()->GetClientIp(), GetUdpSocket()->GetClientPort());

                int old_send_buf_size = 0;
                int old_recv_buf_size = 0;

                int ret = GetSendBufSize(fd, old_send_buf_size);
                cout << LMSG << "GetSendBufSize fd:" << fd << ",ret:" << ret << ",old_send_buf_size:" << old_send_buf_size << endl;

                ret = GetRecvBufSize(fd, old_recv_buf_size);
                cout << LMSG << "GetRecvBufSize fd:" << fd << ",ret:" << ret << ",old_recv_buf_size:" << old_recv_buf_size << endl;

                int new_send_buf_size = 1024*1024*10; // 10MB
                int new_recv_buf_size = 1024*1024*10; // 10MB

                ret = SetSendBufSize(fd, new_send_buf_size, true);
                cout << LMSG << "SetSendBufSize fd:" << fd << ",ret:" << ret << ",new_send_buf_size:" << new_send_buf_size << endl;

                ret = SetRecvBufSize(fd, new_recv_buf_size, true);
                cout << LMSG << "SetRecvBufSize fd:" << fd << ",ret:" << ret << ",new_recv_buf_size:" << new_recv_buf_size << endl;

                ret = GetSendBufSize(fd, new_send_buf_size);
                cout << LMSG << "GetSendBufSize fd:" << fd << ",ret:" << ret << ",new_send_buf_size:" << new_send_buf_size << endl;

                ret = GetRecvBufSize(fd, new_recv_buf_size);
                cout << LMSG << "GetRecvBufSize fd:" << fd << ",ret:" << ret << ",new_recv_buf_size:" << new_recv_buf_size << endl;

                UdpSocket* udp_socket = new UdpSocket(g_epoll, fd, g_webrtc_mgr);
                udp_socket->SetSrcAddr(GetUdpSocket()->GetSrcAddr());
                udp_socket->SetSrcAddrLen(GetUdpSocket()->GetSrcAddrLen());
                udp_socket->EnableRead();

                cout << LMSG << "new webrtc protocol:" << (long)(g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)) << endl;
                g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SetLocalUfrag(g_local_ice_ufrag);
                g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SetLocalPwd(g_local_ice_pwd);
                g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SetRemoteUfrag(g_remote_ice_ufrag);
                g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SetRemotePwd(g_remote_ice_pwd);
                // FIXME:这里可能需要根据角色,比如客户端是上行还是下行来做SetConnectState还是SetAcceptState
#if 0
                g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SendClientHello();
#else
                // datachannel
                g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SetAcceptState();
#endif
                //g_webrtc_mgr->GetOrCreateProtocol(*udp_socket)->SendBindingRequest();
            }
            else
            {
                //SendBindingRequest();
            }
        }
        break;

        case 0x0101:
        {
            cout << LMSG << "Binding Response" << endl;
            SendBindingIndication();
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
    else
    {
		BIO_reset(bio_in_);
        BIO_reset(bio_out_);

        BIO_write(bio_in_, data, len);

        while (BIO_ctrl_pending(bio_in_) > 0)
        {
            cout << LMSG << "DTLS Application data" << endl;
            uint8_t dtls_read_buf[8092];
            int ret = SSL_read(dtls_, dtls_read_buf, sizeof(dtls_read_buf));

            // crc32 test
            {
                uint8_t crc_test[8092];
                memcpy(crc_test, dtls_read_buf, ret);

                BitBuffer bf(crc_test, ret);
                uint32_t unused;
                bf.GetBytes(4, unused);
                bf.GetBytes(4, unused);
                bf.GetBytes(4, unused);

                crc_test[8] = 0x00;
                crc_test[9] = 0x00;
                crc_test[10] = 0x00;
                crc_test[11] = 0x00;

                CRC32 crc_sctp(CRC32_SCTP);
                CRC32 crc_stun(CRC32_STUN);

                uint32_t crc_32_sctp = crc_sctp.GetCrc32(crc_test, ret);
                uint32_t crc_32_stun = crc_stun.GetCrc32(crc_test, ret);

                cout << "in_sctp:" << unused << ",crc_32_sctp:" << crc_32_sctp << ",crc_32_stun:" << crc_32_stun << endl;
            }

            if (ret > 0) 
            {
                cout << LMSG << "dtls read " << ret << " bytes" << endl;
                cout << LMSG << Util::Bin2Hex(dtls_read_buf, ret) << endl;

                {
                    BitBuffer bit_buffer(dtls_read_buf, ret);

                    uint16_t src_port = 0;
                    bit_buffer.GetBytes(2, src_port);

                    uint16_t dst_port = 0;
                    bit_buffer.GetBytes(2, dst_port);

                    uint32_t verification_tag = 0;
                    bit_buffer.GetBytes(4, verification_tag);

                    uint32_t checksum = 0;
                    bit_buffer.GetBytes(4, checksum);

                    uint8_t chunk_type = 0xff;
                    bit_buffer.GetBytes(1, chunk_type);

                    uint8_t chunk_flag = 0xff;
                    bit_buffer.GetBytes(1, chunk_flag);

                    uint16_t chunk_length = 0;
                    bit_buffer.GetBytes(2, chunk_length);

                    cout << LMSG << "src_port:" << src_port << ",dst_port:" << dst_port << ",verification_tag:" << verification_tag
                         << ",checksum:" << checksum << ",chunk_type:" << (int)chunk_type << ",chunk_flag:" << (int)chunk_flag
                         << ",chunk_length:" << chunk_length << endl;

                    switch (chunk_type)
                    {
                        case 0:
                        {
                            uint32_t tsn = 0;
                            bit_buffer.GetBytes(4, tsn);

                            uint16_t stream_id_s = 0;
                            bit_buffer.GetBytes(2, stream_id_s);

                            uint16_t stream_seq_num_n = 0;
                            bit_buffer.GetBytes(2, stream_seq_num_n);

                            uint32_t payload_protocol_id = 0;
                            bit_buffer.GetBytes(4, payload_protocol_id);

                            string user_data = "";
                            bit_buffer.GetString(bit_buffer.BytesLeft(), user_data);
                            cout << LMSG << "recv datachannel msg:[\n" << Util::Bin2Hex(user_data) << "\n]" << endl;
                        }
                        break;

                        case 1:
                        {
                            cout << "SCTP INIT" << endl;
                            bit_buffer.GetBytes(4, initiate_tag_);

                            uint32_t a_rwnd = 0;
                            bit_buffer.GetBytes(4, a_rwnd);

                            uint16_t number_of_outbound_streams = 0;
                            bit_buffer.GetBytes(2, number_of_outbound_streams);

                            uint16_t number_of_inbound_streams = 0;
                            bit_buffer.GetBytes(2, number_of_inbound_streams);

                            uint32_t initial_tsn = 0;
                            bit_buffer.GetBytes(4, initial_tsn);

                            cout << LMSG << "initiate_tag:" << initiate_tag_ << ",a_rwnd:" << a_rwnd << ",number_of_outbound_streams:" 
                                 << number_of_outbound_streams << ",number_of_inbound_streams:" << number_of_inbound_streams << ",initial_tsn:" << initial_tsn << endl;

                            // optional
                            while (bit_buffer.BitsLeft() >= 4)
                            {
                                uint16_t parameter_type = 0;
                                bit_buffer.GetBytes(2, parameter_type);

                                uint16_t parameter_length = 0;
                                bit_buffer.GetBytes(2, parameter_length);

                                string parameter_value;
                                bit_buffer.GetString(parameter_length, parameter_value);

                                cout << LMSG << "parameter_type:" << parameter_type << ",parameter_length:" << parameter_length << endl;
                            }

                            BitStream bs_chunk;
                            bs_chunk.WriteBytes(4, initiate_tag_);
                            bs_chunk.WriteBytes(4, a_rwnd);
                            // 故意反过来的
                            bs_chunk.WriteBytes(2, number_of_inbound_streams);
                            bs_chunk.WriteBytes(2, number_of_outbound_streams);
                            bs_chunk.WriteBytes(4, initial_tsn);
                            // optional state cookie
                            bs_chunk.WriteBytes(2, (uint16_t)0x07);
                            bs_chunk.WriteBytes(2, (uint16_t)8);
                            bs_chunk.WriteBytes(4, (uint32_t)0xB00B1E5);
                            bs_chunk.WriteBytes(2, (uint16_t)0xC000);
                            bs_chunk.WriteBytes(2, (uint16_t)4);

                            BitStream bs;
                            bs.WriteBytes(2, dst_port);
                            bs.WriteBytes(2, src_port);
                            bs.WriteBytes(4, initiate_tag_);// 用initiate_tag替换verification_tag
                            bs.WriteBytes(4, (uint32_t)0x00);
                            bs.WriteBytes(1, (uint32_t)0x02);
                            bs.WriteBytes(1, (uint32_t)0x00);
                            bs.WriteBytes(2, (uint16_t)bs_chunk.SizeInBytes() + 4);
                            bs.WriteData(bs_chunk.SizeInBytes(), bs_chunk.GetData());

                            CRC32 crc32(CRC32_SCTP);
                            //uint32_t crc_32 = crc32.GetCrc32(bs.GetData(), bs.SizeInBytes(), true);
							uint32_t crc_32 = crc32.GetCrc32(bs.GetData(), bs.SizeInBytes());

                            cout << LMSG << "crc32:" << Util::Bin2Hex((const uint8_t*)&crc_32, sizeof(crc_32)) << endl;

                            cout << LMSG << "SCTP INITACK before \n" << Util::Bin2Hex(bs.GetData(), bs.SizeInBytes()) << endl;
                            bs.ReplaceBytes(8, 4, crc_32);

                            cout << LMSG << "SCTP INITACK\n" << Util::Bin2Hex(bs.GetData(), bs.SizeInBytes()) << endl;

                            int ret = SSL_write(dtls_, bs.GetData(), bs.SizeInBytes());
                            cout << LMSG << "ret:" << ret << endl;

							uint8_t dtls_send_buffer[4096];

  							while (BIO_ctrl_pending(bio_out_) > 0) 
                            {
  							    int dtls_send_bytes = BIO_read(bio_out_, dtls_send_buffer, sizeof(dtls_send_buffer));
  							    if (dtls_send_bytes > 0) 
                                {
  							        cout << LMSG << "send dtls:" << dtls_send_bytes << endl;
								    GetUdpSocket()->Send(dtls_send_buffer, dtls_send_bytes);
  							    }   
  							}
                        }
                        break;

                        case 2:
                        {
                        }
                        break;

                        case 3:
                        {
                        }
                        break;

                        case 4:
                        {
                        }
                        break;

                        case 5:
                        {
                        }
                        break;

                        case 6:
                        {
                        }
                        break;

                        case 7:
                        {
                        }
                        break;

                        case 8:
                        {
                        }
                        break;

                        case 9:
                        {
                        }
                        break;

                        case 10:
                        {
                            cout << "SCTP COOKIE" << endl;

                            BitStream bs;
                            bs.WriteBytes(2, dst_port);
                            bs.WriteBytes(2, src_port);
                            bs.WriteBytes(4, initiate_tag_);// 用initiate_tag替换verification_tag
                            bs.WriteBytes(4, (uint32_t)0x00);
                            bs.WriteBytes(1, (uint32_t)0x0B);
                            bs.WriteBytes(1, (uint32_t)0x00);
                            bs.WriteBytes(2, (uint16_t)4);

                            CRC32 crc32(CRC32_SCTP);
                            uint32_t crc_32 = crc32.GetCrc32(bs.GetData(), bs.SizeInBytes());

                            cout << LMSG << "crc32:" << Util::Bin2Hex((const uint8_t*)&crc_32, sizeof(crc_32)) << endl;

                            cout << LMSG << "SCTP COOKIE ACK before \n" << Util::Bin2Hex(bs.GetData(), bs.SizeInBytes()) << endl;
                            bs.ReplaceBytes(8, 4, crc_32);

                            cout << LMSG << "SCTP COOKIE ACK\n" << Util::Bin2Hex(bs.GetData(), bs.SizeInBytes()) << endl;

                            int ret = SSL_write(dtls_, bs.GetData(), bs.SizeInBytes());
                            cout << LMSG << "ret:" << ret << endl;

							uint8_t dtls_send_buffer[4096];

  							while (BIO_ctrl_pending(bio_out_) > 0) 
                            {
  							    int dtls_send_bytes = BIO_read(bio_out_, dtls_send_buffer, sizeof(dtls_send_buffer));
  							    if (dtls_send_bytes > 0) 
                                {
  							        cout << LMSG << "send dtls:" << dtls_send_bytes << endl;
								    GetUdpSocket()->Send(dtls_send_buffer, dtls_send_bytes);
  							    }   
  							}
                        }
                        break;

                        case 11:
                        {
                        }
                        break;

                        case 12:
                        {
                        }
                        break;

                        case 13:
                        {
                        }
                        break;

                        case 14:
                        {
                        }
                        break;

                        default:
                        {
                        }
                        break;
                    }
                }
            }
            else
            {
                int err = SSL_get_error(dtls_, ret); 
                cout << LMSG << "dtls read " << ret << ", err:" << err << endl;
            }
        }
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

    cout << LMSG << "type:" << (int)data[1] << endl;

    if (data[1] == 200 || data[1] == 201 || data[1] == 202 || data[1] == 203 || data[1] == 204)
    {
        int ret = srtp_unprotect_rtcp(srtp_recv_, unprotect_buf, &unprotect_buf_len);
        if (ret == 0)
        {
            cout << LMSG << "srtp_unprotect_rtcp success" << endl;
        }
        else
        {
            cout << LMSG << "srtp_unprotect_rtcp ret:" << ret << endl;
        }

        BitBuffer bit_buffer(unprotect_buf, unprotect_buf_len);

        uint8_t version = 0;
        bit_buffer.GetBits(2, version);

        uint8_t padding = 0;
        bit_buffer.GetBits(1, padding);

        uint8_t reception_report_count = 0;
        bit_buffer.GetBits(5, reception_report_count);

        uint8_t packet_type = 0;
        bit_buffer.GetBits(8, packet_type);

        uint16_t length = 0;
        bit_buffer.GetBytes(2, length);

        uint32_t ssrc = 0;
        bit_buffer.GetBytes(4, ssrc);

        cout << LMSG << "[RTCP Header] # version:" << (int)version
                     << ",padding:" << (int)padding
                     << ",length:" << (int)length
                     << ",packet_type:" << (int)packet_type
                     << ",ssrc:" << ssrc
                     << endl;

        if (packet_type == 201)
        {
            if (bit_buffer.MoreThanBytes(24))
            {
                uint32_t ssrc = 0;
                bit_buffer.GetBytes(4, ssrc);

                uint8_t fraction_lost = 0;
                bit_buffer.GetBytes(1, fraction_lost);

                uint32_t cumulative_number_of_packets_lost = 0;
                bit_buffer.GetBytes(3, cumulative_number_of_packets_lost);

                uint32_t extended_highest_sequence_number_received = 0;
                bit_buffer.GetBytes(4, extended_highest_sequence_number_received);

                uint32_t interarrival_jitter = 0;
                bit_buffer.GetBytes(4, interarrival_jitter);

                uint32_t last_SR = 0;
                bit_buffer.GetBytes(4, last_SR);

                uint32_t delay_since_last_SR = 0;
                bit_buffer.GetBytes(4, delay_since_last_SR);

                cout << LMSG << "[Receiver Report RTCP Packet]"
                             << "ssrc:" << ssrc
                             << ",fraction_lost:" << (int)fraction_lost
                             << ",cumulative_number_of_packets_lost:" << cumulative_number_of_packets_lost
                             << ",extended_highest_sequence_number_received:" << extended_highest_sequence_number_received
                             << ",interarrival_jitter:" << interarrival_jitter
                             << ",last_SR:" << last_SR
                             << ",delay_since_last_SR:" << delay_since_last_SR
                             << endl;
            }
        }
    }
    else
    {
        int ret = srtp_unprotect(srtp_recv_, unprotect_buf, &unprotect_buf_len);
        if (ret == 0)
        {
            cout << LMSG << "srtp_unprotect success" << endl;
        }
        else
        {
            cout << LMSG << "srtp_unprotect ret:" << ret << endl;
        }

        BitBuffer bit_buffer(unprotect_buf, unprotect_buf_len);

        cout << LMSG << "unprotect_buf_len:" << unprotect_buf_len << endl;
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

        if (payload_type == 96)
        {
            RtpDepacketizer::ParsedPayload parsed_payload;

            if (! vp8_depacket_.Parse(&parsed_payload, unprotect_buf + 12, unprotect_buf_len - 12))
            {
                cout << LMSG << "parse vp8 failed" << endl;
                return kError;
            }

            cout << LMSG << "parse vp8 success" << endl;
        }
        else if (payload_type == 98)
        {
            RtpDepacketizer::ParsedPayload parsed_payload;

            if (! vp9_depacket_.Parse(&parsed_payload, unprotect_buf + 12, unprotect_buf_len - 12))
            {
                cout << LMSG << "parse vp9 failed" << endl;
                return kError;
            }

            cout << LMSG << "parse vp9 success"
                         << ",payload_length:" << parsed_payload.payload_length
					  	 << ",frame_type:" << parsed_payload.frame_type
						 << ",is_first_packet:" << parsed_payload.type.Video.isFirstPacket
                         << ",picture_id:" << parsed_payload.type.Video.codecHeader.VP9.picture_id
                         << ",beginning_of_frame:" << parsed_payload.type.Video.codecHeader.VP9.beginning_of_frame
                         << ",end_of_frame:" << parsed_payload.type.Video.codecHeader.VP9.end_of_frame
                         << ",timestamp:" << timestamp
                         << endl;

#ifdef PUBLISH_BACK
            {   
                RtpHeader* rtp_header = (RtpHeader*)unprotect_buf;

                video_publisher_ssrc_ = ssrc;

                rtp_header->setSSRC(3233846889);

                g_webrtc_mgr->__DebugBroadcast(unprotect_buf, unprotect_buf_len);

                /*
                uint8_t protect_buf[1500];
                int protect_buf_len = unprotect_buf_len;
                int ret = ProtectRtp(unprotect_buf, unprotect_buf_len, protect_buf, protect_buf_len);

                if (ret == 0)
                {
                    cout << LMSG << "srtp_protect success" << endl;
                    GetUdpSocket()->Send((const uint8_t*)protect_buf, protect_buf_len);
                }
                else
                {
                    cout << LMSG << "srtp_protect faile:" << ret << endl;
                }
                */
            }
#endif
        }
        else if (payload_type == 111)
        {
            RtpHeader* rtp_header = (RtpHeader*)unprotect_buf;

            audio_publisher_ssrc_ = ssrc;

            rtp_header->setSSRC(3233846889 + 1);

            g_webrtc_mgr->__DebugBroadcast(unprotect_buf, unprotect_buf_len);
        }
    }

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

            send_begin_time_ = Util::GetNowMs();

            g_debug_webrtc = this;

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

void WebrtcProtocol::SendClientHello()
{
    if (! dtls_hello_send_)
    {
        cout << LMSG << "dtls send clienthello" << endl;

        dtls_hello_send_ = true;

#ifdef USE_MEDIA_INPUT
        if (media_input_ == NULL)
        {
            media_input_ = new MediaInput();
        }

#ifdef USE_VP8_WEBM
        media_input_->Open("input_vp8.webm");
#else
        media_input_->Open("input_vp9.webm");
#endif
        ++media_input_open_count_;
        media_input_read_video_frame_count = 0;
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

void WebrtcProtocol::SetAcceptState()
{
    if (! dtls_hello_send_)
    {
        cout << LMSG << "dtls send clienthello" << endl;

        dtls_hello_send_ = true;

#ifdef USE_MEDIA_INPUT
        if (media_input_ == NULL)
        {
            media_input_ = new MediaInput();
        }

#ifdef USE_VP8_WEBM
        media_input_->Open("input_vp8.webm");
#else
        media_input_->Open("input_vp9.webm");
#endif
        ++media_input_open_count_;
        media_input_read_video_frame_count = 0;
#endif 

        if (dtls_ == NULL)
        {
            dtls_ = SSL_new(g_dtls_ctx);

			SSL_set_accept_state(dtls_);

        	bio_in_  = BIO_new(BIO_s_mem());
        	bio_out_ = BIO_new(BIO_s_mem());

        	SSL_set_bio(dtls_, bio_in_, bio_out_);

            Handshake();
        }
    }
}

int WebrtcProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return 0;
}

int WebrtcProtocol::EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return 0;
#ifdef USE_MEDIA_INPUT

#ifdef PUBLISH_BACK

    if (dtls_handshake_done_ && count % 50 == 0)
    {
        uint32_t pli_ssrc[4][2];
        pli_ssrc[0][0] = video_publisher_ssrc_;
        pli_ssrc[0][1] = video_publisher_ssrc_;
        pli_ssrc[1][0] = 3233846889;
        pli_ssrc[1][1] = 3233846889;
        pli_ssrc[2][0] = video_publisher_ssrc_;
        pli_ssrc[2][1] = 3233846889;
        pli_ssrc[3][0] = 3233846889;
        pli_ssrc[3][1] = video_publisher_ssrc_;

        for (int i = 0; i != 4; ++i)
        {
            BitStream bs_pli;

            bs_pli.WriteBits(2, 0x02);
            bs_pli.WriteBits(1, 0x00);
            bs_pli.WriteBits(5, 0x01);
            bs_pli.WriteBytes(1, 206);
            bs_pli.WriteBytes(2, 2);
            bs_pli.WriteBytes(4, pli_ssrc[i][0]);
            bs_pli.WriteBytes(4, pli_ssrc[i][1]);

            GetUdpSocket()->Send(bs_pli.GetData(), bs_pli.SizeInBytes());
        }

        uint32_t fir_ssrc[8][3];
        fir_ssrc[0][0] = video_publisher_ssrc_;
        fir_ssrc[0][1] = video_publisher_ssrc_;
        fir_ssrc[0][2] = video_publisher_ssrc_;

        fir_ssrc[1][0] = video_publisher_ssrc_;
        fir_ssrc[1][1] = video_publisher_ssrc_;
        fir_ssrc[1][2] = 3233846889;

        fir_ssrc[2][0] = video_publisher_ssrc_;
        fir_ssrc[2][1] = 3233846889;
        fir_ssrc[2][2] = 3233846889;

        fir_ssrc[3][0] = video_publisher_ssrc_;
        fir_ssrc[3][1] = 3233846889;
        fir_ssrc[3][2] = video_publisher_ssrc_;

        fir_ssrc[4][0] = 3233846889;
        fir_ssrc[4][1] = 3233846889;
        fir_ssrc[4][2] = 3233846889;

        fir_ssrc[5][0] = 3233846889;
        fir_ssrc[5][1] = 3233846889;
        fir_ssrc[5][2] = video_publisher_ssrc_;

        fir_ssrc[6][0] = 3233846889;
        fir_ssrc[6][1] = video_publisher_ssrc_;
        fir_ssrc[6][2] = 3233846889;

        fir_ssrc[7][0] = 3233846889;
        fir_ssrc[7][1] = video_publisher_ssrc_;
        fir_ssrc[7][2] = video_publisher_ssrc_;

        for (int i = 0; i != 8; ++i)
        {
            BitStream bs_fir;

            bs_fir.WriteBits(2, 0x02);
            bs_fir.WriteBits(1, 0x00);
            bs_fir.WriteBits(5, 0x04);  // FIR
            bs_fir.WriteBytes(1, 206);
            bs_fir.WriteBytes(2, 4);
            bs_fir.WriteBytes(4, fir_ssrc[i][0]);
            bs_fir.WriteBytes(4, fir_ssrc[i][1]);
            bs_fir.WriteBytes(4, fir_ssrc[i][2]);
            static uint8_t seq_nr = 1;
            bs_fir.WriteBytes(1, ++seq_nr);
            bs_fir.WriteBytes(3, 0x000000);

            GetUdpSocket()->Send(bs_fir.GetData(), bs_fir.SizeInBytes());
        }

		RtcpHeader rtcp_pli;
    	rtcp_pli.setPacketType(RTCP_PS_Feedback_PT);
    	rtcp_pli.setBlockCount(1);
#if 0
    	rtcp_pli.setSSRC(3233846889);
    	rtcp_pli.setSourceSSRC(video_publisher_ssrc_);
#else
    	rtcp_pli.setSSRC(video_publisher_ssrc_);
    	rtcp_pli.setSourceSSRC(3233846889);
#endif
    	rtcp_pli.setLength(2);

    	uint8_t *buf = (uint8_t*)(&rtcp_pli);
    	size_t len = (rtcp_pli.getLength() + 1)*4;

        GetUdpSocket()->Send(buf, len);

        cout << LMSG << "send pli " << endl;
    }

#endif

    cout << LMSG << "count:" << count << endl;

    if (dtls_handshake_done_ && media_input_ != NULL)
    {
        uint64_t send_delta = now_in_ms - send_begin_time_;

        cout << LMSG << "send_delta:" << send_delta << ",timestamp_:" << timestamp_ << endl;

        // FIXME:这种用法如果webrtc_mgr里面有多个webrtc_protocol的话, 会卡死, 大概是因为UDP发包队列的限制
        while (send_delta >= timestamp_)
        {
            uint8_t* frame_data = NULL;
            int frame_len = 0;
            int flag = 0;
            bool is_video = false;

            int ret = media_input_->ReadFrame(frame_data, frame_len, flag, is_video, timestamp_);

            if (ret == 0)
            {
                delete media_input_;

                media_input_ = new MediaInput();
#ifdef USE_VP8_WEBM
                media_input_->Open("input_vp8.webm");
#else
                media_input_->Open("input_vp9.webm");
#endif

                ++media_input_open_count_;
                media_input_read_video_frame_count = 0;

                cout << LMSG << "timestamp_base_:" << timestamp_base_ << "=>" << (timestamp_base_ + timestamp_) << endl;
                timestamp_base_ += timestamp_;
            }
            else if (ret < 0)
            {
                return 0;
            }

            if (timestamp_ > UINT32_MAX)
            {
                cout << LMSG << "first media frame, timestamp_:" << timestamp_ << endl;
                timestamp_ = 0;
            }

            ++media_input_read_video_frame_count;

            cout << LMSG << "is_video:" << is_video << ",timestamp_:" << timestamp_ << endl;

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

                        if (media_input_open_count_ > 1)
                        {
                            rtp_header.setTimestamp((uint32_t)(timestamp_ + timestamp_base_) * 90);
                        }
                        else
                        {
                            rtp_header.setTimestamp((uint32_t)timestamp_ * 90);
                        }

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

                if (media_input_open_count_ > 1)
                {
            	    rtp_header.setTimestamp((uint32_t)(timestamp_ + timestamp_base_) * 48);
                }
                else
                {
            	    rtp_header.setTimestamp((uint32_t)timestamp_ * 48);
                }

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

#endif

#ifdef USE_TRANSCODER
    if (dtls_handshake_done_)
    {
        uint64_t send_delta = now_in_ms - send_begin_time_;

        cout << LMSG << "send_delta:" << send_delta << ",timestamp_:" << timestamp_ << endl;

        if (g_media_queue.size() >= 600)
        {
            cout << LMSG << "g_media_queue.size() = " << g_media_queue.size() << endl;

            while (send_delta >= timestamp_)
            {
                static int index = 0;
                MediaPacket& media_packet = g_media_queue[index % g_media_queue.size()];
                ++index;
                timestamp_ = media_packet.dts_;

                if (timestamp_ > UINT32_MAX)
                {
                    cout << LMSG << "first media frame, timestamp_:" << timestamp_ << endl;
                    timestamp_ = 0;
                }

                if (media_packet.IsVideo())
                {
                    SendVideoData((const uint8_t*)(media_packet.data_.data()), media_packet.data_.size(), media_packet.dts_, media_packet.flag_);
                }
                else if (media_packet.IsAudio())
                {
                    SendAudioData((const uint8_t*)(media_packet.data_.data()), media_packet.data_.size(), media_packet.dts_, media_packet.flag_);
                }

                //g_media_queue.pop_front();
            }
        }
    }
#endif

    return kSuccess;
}

void WebrtcProtocol::SendBindingRequest()
{
    uint32_t magic_cookie = 0x2112A442;
    string transcation_id = Util::GenRandom(12);

    BitStream binding_request;
    string username = remote_ufrag_ + ":" + local_ufrag_;
    binding_request.WriteBytes(2, 0x0006); // USERNAME
    binding_request.WriteBytes(2, username.size());
    binding_request.WriteData(username.size(), (const uint8_t*)username.data());

    binding_request.WriteBytes(2, 0x8029); // ICE_CONTROLLED
    binding_request.WriteBytes(2, 8);
    uint64_t tie_breaker = 123;
    binding_request.WriteBytes(8, tie_breaker);

    binding_request.WriteBytes(2, 0x0025); // PRIORITY
    binding_request.WriteBytes(2, 4);
    uint32_t priority = get_host_priority(0xFFFF, true);
    binding_request.WriteBytes(4, priority);

    uint8_t hmac[20] = {0};
    {
        BitStream hmac_input;
        hmac_input.WriteBytes(2, 0x0001); // Binding Request
        hmac_input.WriteBytes(2, binding_request.SizeInBytes() + 4 + 20);
        hmac_input.WriteBytes(4, magic_cookie);
        hmac_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
        hmac_input.WriteData(binding_request.SizeInBytes(), binding_request.GetData());
        unsigned int out_len = 0;
        HmacEncode("sha1", remote_pwd_.data(), remote_pwd_.size(), (const char*)hmac_input.GetData(), hmac_input.SizeInBytes(), hmac, out_len);

        cout << LMSG << "remote_pwd_:" << remote_pwd_ << endl;
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
        CRC32 crc32(CRC32_STUN);
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

void WebrtcProtocol::SendBindingIndication()
{
    uint32_t magic_cookie = 0x2112A442;
    string transcation_id = Util::GenRandom(12);

    BitStream binding_indication;

    uint8_t hmac[20] = {0};
    {
        BitStream hmac_input;
        hmac_input.WriteBytes(2, 0x0011); // Binding Indication
        hmac_input.WriteBytes(2, 4 + 20);
        hmac_input.WriteBytes(4, magic_cookie);
        hmac_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
        unsigned int out_len = 0;
        HmacEncode("sha1", remote_pwd_.data(), remote_pwd_.size(), (const char*)hmac_input.GetData(), hmac_input.SizeInBytes(), hmac, out_len);

        cout << LMSG << "remote_pwd_:" << remote_pwd_ << endl;
        cout << LMSG << "hamc out_len:" << out_len << endl;
    }

    binding_indication.WriteBytes(2, 0x0008);
    binding_indication.WriteBytes(2, 20);
    binding_indication.WriteData(20, hmac);

    uint32_t crc_32 = 0;
    {
        BitStream crc32_input;
        crc32_input.WriteBytes(2, 0x0011); // Binding Indication
        crc32_input.WriteBytes(2, binding_indication.SizeInBytes() + 8);
        crc32_input.WriteBytes(4, magic_cookie);
        crc32_input.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
        crc32_input.WriteData(binding_indication.SizeInBytes(), binding_indication.GetData());
        CRC32 crc32(CRC32_STUN);
        cout << LMSG << "my crc32 input:" << Util::Bin2Hex(crc32_input.GetData(), crc32_input.SizeInBytes()) << endl;
        crc_32 = crc32.GetCrc32(crc32_input.GetData(), crc32_input.SizeInBytes());
        cout << LMSG << "crc32:" << crc_32 << endl;
        crc_32 = crc_32 ^ 0x5354554E;
        cout << LMSG << "crc32:" << crc_32 << endl;
    }

    binding_indication.WriteBytes(2, 0x8028);
    binding_indication.WriteBytes(2, 4);
    binding_indication.WriteBytes(4, crc_32);

    BitStream binding_indication_header;
    binding_indication_header.WriteBytes(2, 0x0011); // Binding Indication
    binding_indication_header.WriteBytes(2, binding_indication.SizeInBytes());
    binding_indication_header.WriteBytes(4, magic_cookie);
    binding_indication_header.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
    binding_indication_header.WriteData(binding_indication.SizeInBytes(), binding_indication.GetData());

    cout << LMSG << "myself send binding_indication\n" 
         << Util::Bin2Hex(binding_indication_header.GetData(), binding_indication_header.SizeInBytes()) << endl;


    GetUdpSocket()->Send(binding_indication_header.GetData(), binding_indication_header.SizeInBytes());
}
