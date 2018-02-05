#include <iostream>
#include <map>

#include "openssl/ssl.h"

#include "bit_stream.h"
#include "common_define.h"
#include "crc32.h"
#include "webrtc_protocol.h"
#include "io_buffer.h"
#include "socket_util.h"
#include "udp_socket.h"

using namespace socket_util;
using namespace std;

WebrtcProtocol::WebrtcProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket)
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
            cout << LMSG << "rtp" << endl;
   		}   
   		else if ((data[0] >= 20) && (data[0] <= 64))
   		{   
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
         //<< ",transcation_id:" << Util::Bin2Hex(transcation_id)
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
            cout << LMSG << "Binding Request" << endl;

            uint32_t magic_cookie = 0x2112A442;

            BitStream binding_response;

            binding_response.WriteBytes(2, 0x0006); // USERNAME
            binding_response.WriteBytes(2, username.size());
            binding_response.WriteBytes(username.size(), (const uint8_t*)username.data());

            binding_response.WriteBytes(2, 0x0020);
            binding_response.WriteBytes(2, 8);
            binding_response.WriteBytes(1, 0x00);
            binding_response.WriteBytes(1, 0x01); // IPv4
            binding_response.WriteBytes(2, (11445 ^ (magic_cookie >> 16)));

            uint32_t ip_num;
            IpStr2Num("192.168.247.128", ip_num);
            binding_response.WriteBytes(4, htobe32(htobe32(magic_cookie) ^ ip_num));

            uint8_t hmac[20] = {0};
            SHA1(binding_response.GetData(), binding_response.SizeInBytes(), hmac);

            binding_response.WriteBytes(2, 0x0008);
            binding_response.WriteBytes(2, 20);
            binding_response.WriteData(20, hmac);

            CRC32 crc32;
            uint32_t crc_32 = crc32.GetCrc32(binding_response.GetData(), binding_response.SizeInBytes());
            crc_32 = crc_32 ^ 0x5354554E;

            binding_response.WriteBytes(2, 0x8028);
            binding_response.WriteBytes(2, 4);
            binding_response.WriteBytes(4, crc_32);

            BitStream binding_response_header;
            binding_response_header.WriteBytes(2, 0x0101); // Binding Response
            binding_response_header.WriteBytes(2, binding_response.SizeInBytes());
            binding_response_header.WriteBytes(4, magic_cookie);
            binding_response_header.WriteData(transcation_id.size(), (const uint8_t*)transcation_id.data());
            binding_response_header.WriteData(binding_response.SizeInBytes(), binding_response.GetData());

            GetUdpSocket()->Send(binding_response_header.GetData(), binding_response_header.SizeInBytes());
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

int WebrtcProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return kSuccess;
}
