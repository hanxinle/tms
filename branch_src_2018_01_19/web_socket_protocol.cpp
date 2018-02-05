#include <iostream>
#include <map>

#include "base_64.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "common_define.h"
#include "global.h"
#include "web_socket_protocol.h"
#include "io_buffer.h"
#include "tcp_socket.h"

using namespace std;

WebSocketProtocol::WebSocketProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket),
    upgrade_(false)
{
}

WebSocketProtocol::~WebSocketProtocol()
{
}

/*
	 0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+

	Opcode:  4 bits
      Defines the interpretation of the "Payload data".  If an unknown
      opcode is received, the receiving endpoint MUST _Fail the
      WebSocket Connection_.  The following values are defined.

      *  %x0 denotes a continuation frame
      *  %x1 denotes a text frame
      *  %x2 denotes a binary frame
      *  %x3-7 are reserved for further non-control frames
      *  %x8 denotes a connection close
      *  %x9 denotes a ping
      *  %xA denotes a pong
      *  %xB-F are reserved for further control frames
*/
int WebSocketProtocol::Parse(IoBuffer& io_buffer)
{
    int http_size = 0;

    if (! upgrade_)
    {
		uint8_t* data = NULL;

    	int size = io_buffer.Peek(data, 0, io_buffer.Size());

    	int r_pos = -1; // '\r'
    	int n_pos = -1; // '\n'
    	int m_pos = -1; // ':'

    	bool key_value = false; // false:key, true:value

    	string key;
    	string value;

    	map<string, string> header;

    	for (int i = 0; i != size; ++i)
    	{
    	    if (data[i] == '\r')
    	    {
    	        r_pos = i;
    	    }
    	    else if (data[i] == '\n')
    	    {
    	        if (i == r_pos + 1)
    	        {
    	            if (i == n_pos + 2) // \r\n\r\n
    	            {
    	                cout << LMSG << "http done" << endl;

                        upgrade_ = true;
                        http_size = i + 1;

                        io_buffer.Skip(http_size);

                        auto iter_ws_key = header.find("Sec-WebSocket-Key");
                        if (iter_ws_key == header.end())
                        {
                            return kClose;
                        }

                        string web_socket_key = iter_ws_key->second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

						string web_socket_rsp = "HTTP/1.1 101 Switching Protocols\r\n"
												"Server: trs\r\n"
												"Connection: upgrade\r\n"
												"Sec-WebSocket-Accept: ";

                    	string web_socket_key_sha;
                    	web_socket_key_sha.resize(20);

                        SHA1((const unsigned char*)(web_socket_key.data()), web_socket_key.length(), (unsigned char*)web_socket_key_sha.data());

                    	string web_socket_key_sha_base64;
                        Base64::Encode(web_socket_key_sha, web_socket_key_sha_base64);
                        web_socket_rsp += web_socket_key_sha_base64;
                        web_socket_rsp += "\r\nUpgrade: websocket\r\n\r\n";


						GetTcpSocket()->Send((const uint8_t*)web_socket_rsp.data(), web_socket_rsp.size());

    	                return kSuccess;
    	            }
    	            else // \r\n
    	            {
    	                key_value = false;

    	                cout << LMSG << key << ":" << value << endl;

    	                if (key.find("GET") != string::npos)
    	                {
    	                    //GET /test.flv HTTP/1.1
    	                    int s_count = 0;
    	                    int x_count = 0; // /
    	                    int d_count = 0; // .
    	                    for (const auto& ch : key)
    	                    {
    	                        if (ch == ' ')
    	                        {
    	                            ++s_count;
    	                        }
    	                        else if (ch == '/')
    	                        {
    	                            ++x_count;
    	                        }
    	                        else if (ch == '.')
    	                        {
    	                            ++d_count;
    	                        }
    	                        else
    	                        {
    	                        }
    	                    }
    	                }

    	                header[key] = value;
    	                key.clear();
    	                value.clear();
    	            }
    	        }

    	        n_pos = i;
    	    }
    	    else if (data[i] == ':')
    	    {
    	        m_pos = i;
    	        key_value = true;
    	    }
    	    else if (data[i] == ' ')
    	    {
    	        if (m_pos + 1 == i)
    	        {
    	        }
    	        else
    	        {
    	            if (key_value)
    	            {
    	                value += (char)data[i];
    	            }
    	            else
    	            {
    	                key += (char)data[i];
    	            }
    	        }
    	    }
    	    else
    	    {
    	        if (key_value)
    	        {
    	            value += (char)data[i];
    	        }
    	        else
    	        {
    	            key += (char)data[i];
    	        }
    	    }
    	}

    	return kNoEnoughData;
    }
    else
    {
        // peek
        {
	    	uint8_t* data = NULL;
        	size_t len = 0;
        	len = io_buffer.Peek(data, 0, io_buffer.Size());

        	cout << LMSG << Util::Bin2Hex(data, len) << endl;
        }

        uint32_t header_len = kWebSocketProtocolHeaderSize;
	    if (io_buffer.Size() < header_len)
        {   
            return kNoEnoughData;
        }   

        uint8_t* header = NULL;

        io_buffer.Peek(header, 0, 2);

        uint8_t fin = header[0] & 0x80;
        uint8_t opcode = header[0] & 0x0F;
        uint8_t mask = header[1] & 0x80;

        uint8_t payload_len = header[1] & 0x7F;

        uint8_t peek_len = 0;
        if (payload_len == 126)
        {
            peek_len = 2;
        }
        else if (payload_len == 127)
        {
            peek_len = 8;
        }

        header_len += peek_len;

        uint8_t mask_len = 0;

        if (mask)
        {
            mask_len = 4;
        }

	    if (io_buffer.Size() < header_len)
        {
            return kNoEnoughData;
        }

        uint8_t* peek = NULL;
        io_buffer.Peek(peek, 2, peek_len);

        uint64_t extended_payload_length = payload_len;

        if (peek_len > 0)
        {
            BitBuffer bit_buffer(peek, peek_len);
            bit_buffer.GetBytes(peek_len, extended_payload_length);
        }

        cout << LMSG << "fin:" << (int)fin << ",opcode:" << (int)opcode << ",mask:" << (int)mask << ",payload_len:" << (int)payload_len 
             << ",peek_len:" << (int)peek_len <<",extended_payload_length:" << extended_payload_length << endl;

        if (io_buffer.Size() < header_len + extended_payload_length + mask_len)
        {
            return kNoEnoughData;
        }

        io_buffer.Skip(header_len);
        uint8_t* data = NULL;
        io_buffer.Read(data, extended_payload_length + mask_len);

        if (mask)
        {
#if 0       // slow algorithm
            uint8_t* mask_octet = data;
            data += 4;

            for (uint64_t i = 0; i < extended_payload_length; ++i)
            {
                data[i] = (data[i] ^ mask_octet[i % 4]);
            }
#else       // faster algorithm
			uint8_t* mask_octet = data;
            uint32_t mask_4bytes = (mask_octet[0] << 24) | (mask_octet[1] << 16) | (mask_octet[2] << 8) | (mask_octet[3]);
            // 用主机字节序进行XOR, 因为后面有一个uint32_t*指针读取写入的操作
            mask_4bytes = be32toh(mask_4bytes);

            data += 4;

            for (uint64_t i = 0; i < extended_payload_length / 4; ++i) 
            {   
                uint32_t* data_4bytes = (uint32_t*)(data + 4 * i); 
                *data_4bytes = (*data_4bytes) ^ mask_4bytes;
            }   

            for (uint64_t i = extended_payload_length - extended_payload_length % 4; i < extended_payload_length; ++i)
            {   
                data[i] = (data[i] ^ mask_octet[i % 4]);
            }
#endif
        }
        else
        {
        }

        cout << LMSG << "payload:" << Util::Bin2Hex(data, extended_payload_length) << endl;

        // FIXME: sdp 
        if (extended_payload_length >= 2 && data[0] == 'v' && data[1] == '=')
        {
            string webrtc_test_sdp = Util::ReadFile("webrtc_test.sdp");

            if (webrtc_test_sdp.empty())
            {
                return kClose;
            }

            Util::Replace(webrtc_test_sdp, "a=fingerprint:sha-256\r\n", "a=fingerprint:sha-256 " + g_dtls_fingerprint + "\r\n");

			auto ufrag = Util::GenRandom(15);
            //auto pwd = Util::GenRandom(22);
            string pwd = "xiaozhihongxiaozhihong";

            Util::Replace(webrtc_test_sdp, "a=ice-ufrag:xxx\r\n", "a=ice-ufrag:" + ufrag + "\r\n");
            Util::Replace(webrtc_test_sdp, "a=ice-pwd:xxx\r\n", "a=ice-pwd:" + pwd + "\r\n");
            Util::Replace(webrtc_test_sdp, "xxx.xxx.xxx.xxx:what", "192.168.247.128 11445");

            string sdp_answer = "{\"sdpAnswer\":\"" + webrtc_test_sdp + "\"}";

            Util::Replace(sdp_answer, "\r\n", "\\r\\n");

            cout << LMSG << sdp_answer << endl;

            Send((const uint8_t*)sdp_answer.data(), sdp_answer.size());
        }

        return kSuccess;
    }


    // avoid warning
    return kClose;
}

int WebSocketProtocol::Send(const uint8_t* data, const size_t& len)
{
    BitStream bs;

    bs.WriteBits(1, 0x01);
    bs.WriteBits(3, 0x00);
    
    bs.WriteBits(4, 0x01);

    bs.WriteBits(1, 0x00);

    if (len > 125)
    {
        if (len < ((1UL<<31)-1))
        {
            bs.WriteBits(7, 126);
            bs.WriteBytes(2, len);
        }
        else
        {
            bs.WriteBits(7, 127);
            bs.WriteBytes(8, len);
        }
    }
    else
    {
        bs.WriteBits(7, len);
    }

    bs.WriteData(len, data);

    GetTcpSocket()->Send(bs.GetData(), bs.SizeInBytes());

    return kSuccess;
}

int WebSocketProtocol::OnStop()
{
    return 0;
}

int WebSocketProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    return 0;
}
