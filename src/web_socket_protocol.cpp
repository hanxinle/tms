#include "web_socket_protocol.h"

#include <iostream>
#include <map>

#include "base_64.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "common_define.h"
#include "global.h"
#include "io_buffer.h"
#include "rapidjson/document.h"
#include "sdp.h"
#include "tcp_socket.h"
#include "webrtc_session_mgr.h"

WebSocketProtocol::WebSocketProtocol(IoLoop* io_loop, Fd* socket)
    : io_loop_(io_loop), socket_(socket), upgrade_(false) {}

WebSocketProtocol::~WebSocketProtocol() {}

int WebSocketProtocol::HandleRead(IoBuffer& io_buffer, Fd& socket) {
  int ret = kError;
  do {
    ret = Parse(io_buffer);
  } while (ret == kSuccess);

  return ret;
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
     |                               |Masking-key, if MASK std::set to 1  |
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
int WebSocketProtocol::Parse(IoBuffer& io_buffer) {
  int http_size = 0;

  if (!upgrade_) {
    int ret = http_parse_.Decode(io_buffer);
    if (ret == kSuccess) {
      upgrade_ = true;

      std::string web_socket_key = "";

      if (!http_parse_.GetHeaderKeyValue("Sec-WebSocket-Key", web_socket_key)) {
        return kClose;
      }

      web_socket_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

      std::string web_socket_rsp =
          "HTTP/1.1 101 Switching Protocols\r\n"
          "Server: tms\r\n"
          "Connection: upgrade\r\n"
          "Sec-WebSocket-Accept: ";

      std::string web_socket_key_sha;
      web_socket_key_sha.resize(20);

      SHA1((const unsigned char*)(web_socket_key.data()),
           web_socket_key.length(), (unsigned char*)web_socket_key_sha.data());

      std::string web_socket_key_sha_base64;
      Base64::Encode(web_socket_key_sha, web_socket_key_sha_base64);
      web_socket_rsp += web_socket_key_sha_base64;
      web_socket_rsp += "\r\nUpgrade: websocket\r\n\r\n";

      GetTcpSocket()->Send((const uint8_t*)web_socket_rsp.data(),
                           web_socket_rsp.size());
    }

    return ret;
  } else {
    uint32_t header_len = kWebSocketProtocolHeaderSize;
    if (io_buffer.Size() < header_len) {
      return kNoEnoughData;
    }

    uint8_t* header = NULL;

    io_buffer.Peek(header, 0, 2);

    uint8_t fin = header[0] & 0x80;
    uint8_t opcode = header[0] & 0x0F;
    uint8_t mask = header[1] & 0x80;

    uint8_t payload_len = header[1] & 0x7F;

    uint8_t peek_len = 0;
    if (payload_len == 126) {
      peek_len = 2;
    } else if (payload_len == 127) {
      peek_len = 8;
    }

    header_len += peek_len;

    uint8_t mask_len = 0;

    if (mask) {
      mask_len = 4;
    }

    if (io_buffer.Size() < header_len) {
      return kNoEnoughData;
    }

    uint8_t* peek = NULL;
    io_buffer.Peek(peek, 2, peek_len);

    uint64_t extended_payload_length = payload_len;

    if (peek_len > 0) {
      BitBuffer bit_buffer(peek, peek_len);
      bit_buffer.GetBytes(peek_len, extended_payload_length);
    }

    std::cout << LMSG << "fin:" << (int)fin << ",opcode:" << (int)opcode
              << ",mask:" << (int)mask << ",payload_len:" << (int)payload_len
              << ",peek_len:" << (int)peek_len
              << ",extended_payload_length:" << extended_payload_length
              << std::endl;

    if (io_buffer.Size() < header_len + extended_payload_length + mask_len) {
      return kNoEnoughData;
    }

    io_buffer.Skip(header_len);
    uint8_t* data = NULL;
    io_buffer.Read(data, extended_payload_length + mask_len);

    if (mask) {
      uint8_t* mask_octet = data;
      uint32_t mask_4bytes = (mask_octet[0] << 24) | (mask_octet[1] << 16) |
                             (mask_octet[2] << 8) | (mask_octet[3]);
      // 用主机字节序进行XOR, 因为后面有一个uint32_t*指针读取写入的操作
#if defined(__APPLE__)
      mask_4bytes = ntohl(mask_4bytes);
#else
      mask_4bytes = be32toh(mask_4bytes);
#endif

      data += 4;

      for (uint64_t i = 0; i < extended_payload_length / 4; ++i) {
        uint32_t* data_4bytes = (uint32_t*)(data + 4 * i);
        *data_4bytes = (*data_4bytes) ^ mask_4bytes;
      }

      for (uint64_t i = extended_payload_length - extended_payload_length % 4;
           i < extended_payload_length; ++i) {
        data[i] = (data[i] ^ mask_octet[i % 4]);
      }
    } else {
    }

    std::cout << LMSG << "websocket payload:\n"
              << Util::Bin2Hex(data, extended_payload_length) << std::endl;

    rapidjson::Document doc;
    rapidjson::ParseResult ok =
        doc.Parse((const char*)data, extended_payload_length);
    if (!ok) {
      std::cout << LMSG << "invalid json" << std::endl;
      return kError;
    }

    rapidjson::Value& sdp = doc["sdp"];
    std::string remote_sdp(sdp.GetString());

    Sdp sdp_parser;
    if (sdp_parser.parse(remote_sdp) != 0) {
      std::cout << LMSG << "parse sdp failed" << std::endl;
      return kError;
    }

    std::vector<std::string> sdp_line = Util::SepStr(remote_sdp, "\r\n");

    std::cout << LMSG << "==================== remote sdp ===================="
              << std::endl;
    for (const auto& line : sdp_line) {
      std::cout << LMSG << line << std::endl;

      if (line.find("a=ice-ufrag") != std::string::npos) {
        std::vector<std::string> tmp = Util::SepStr(line, ":");

        if (tmp.size() == 2) {
          g_remote_ice_ufrag = tmp[1];
        }
      } else if (line.find("a=ice-pwd") != std::string::npos) {
        std::vector<std::string> tmp = Util::SepStr(line, ":");

        if (tmp.size() == 2) {
          g_remote_ice_pwd = tmp[1];
        }
      }
    }

    std::cout << LMSG
              << "g_remote_ice_ufrag:" << Util::Bin2Hex(g_remote_ice_ufrag)
              << std::endl;
    std::cout << LMSG << "g_remote_ice_pwd:" << Util::Bin2Hex(g_remote_ice_pwd)
              << std::endl;

    std::string sdp_file =
        http_parse_.GetFileName() + "." + http_parse_.GetFileType();

    std::cout << LMSG << "sdp file:" << sdp_file << std::endl;

    std::string webrtc_test_sdp = Util::ReadFile(sdp_file);

    if (webrtc_test_sdp.empty()) {
      return kClose;
    }

    Util::Replace(webrtc_test_sdp, "a=fingerprint:sha-256\r\n",
                  "a=fingerprint:sha-256 " + g_dtls_fingerprint + "\r\n");

    g_local_ice_ufrag = Util::GenRandom(8);
    g_local_ice_pwd = Util::GenRandom(32);

    std::cout << LMSG
              << "g_local_ice_ufrag:" << Util::Bin2Hex(g_local_ice_ufrag)
              << std::endl;
    std::cout << LMSG << "g_local_ice_pwd:" << Util::Bin2Hex(g_local_ice_pwd)
              << std::endl;

    Util::Replace(webrtc_test_sdp, "a=ice-ufrag:xxx\r\n",
                  "a=ice-ufrag:" + g_local_ice_ufrag + "\r\n");
    Util::Replace(webrtc_test_sdp, "a=ice-pwd:xxx\r\n",
                  "a=ice-pwd:" + g_local_ice_pwd + "\r\n");
    Util::Replace(webrtc_test_sdp, "xxx.xxx.xxx.xxx:what",
                  g_server_ip + " 11445");
    Util::Replace(webrtc_test_sdp, "xxx.xxx.xxx.xxx", g_server_ip);
    Util::Replace(webrtc_test_sdp, "xxx_port", "11445");

// a=sendrecv sdp中这个影响chrome推流
#if 0
        // 标准流程都是这么做的, sdpMid需要跟sdp中的mid:对齐, datachannel一定要走到这里来
        std::string candidate = R"(candidate":"candidate:1 1 udp 2115783679 xxx.xxx.xxx.xxx:what typ host generation 0 ufrag )" + g_local_ice_ufrag + R"( netwrok-cost 50", "sdpMid":"0","sdpMLineIndex":0)";
        Util::Replace(candidate, "xxx.xxx.xxx.xxx:what", g_server_ip + " 11445");
        std::string sdp_answer = "{\"sdpAnswer\":\"" + webrtc_test_sdp + "\", \"candidate\":{" + "\"" + candidate + "}}";
#else
    std::string sdp_answer = "{\"sdpAnswer\":\"" + webrtc_test_sdp + "\"}";
#endif

    std::cout << LMSG << "==================== local sdp ===================="
              << std::endl;
    sdp_line = Util::SepStr(webrtc_test_sdp, "\r\n");
    for (const auto& line : sdp_line) {
      std::cout << LMSG << line << std::endl;
    }

    Util::Replace(sdp_answer, "\r\n", "\\r\\n");

    std::cout << LMSG << sdp_answer << std::endl;

    Send((const uint8_t*)sdp_answer.data(), sdp_answer.size());

    SessionInfo session_info;
    session_info.remote_ufrag = g_remote_ice_ufrag;
    session_info.remote_pwd = g_remote_ice_pwd;
    session_info.local_ufrag = g_local_ice_ufrag;
    session_info.local_pwd = g_local_ice_pwd;
    session_info.app = doc["app"].GetString();
    session_info.stream = doc["stream"].GetString();

    g_webrtc_session_mgr.AddSession(session_info.remote_ufrag, session_info);

    return kSuccess;
  }

  // avoid warning
  return kClose;
}

int WebSocketProtocol::Send(const uint8_t* data, const size_t& len) {
  BitStream bs;

  bs.WriteBits(1, 0x01);
  bs.WriteBits(3, 0x00);

  bs.WriteBits(4, 0x01);

  bs.WriteBits(1, 0x00);

  if (len > 125) {
    if (len < ((1UL << 31) - 1)) {
      bs.WriteBits(7, 126);
      bs.WriteBytes(2, len);
    } else {
      bs.WriteBits(7, 127);
      bs.WriteBytes(8, len);
    }
  } else {
    bs.WriteBits(7, len);
  }

  bs.WriteData(len, data);

  GetTcpSocket()->Send(bs.GetData(), bs.SizeInBytes());

  return kSuccess;
}

int WebSocketProtocol::EveryNSecond(const uint64_t& now_in_ms,
                                    const uint32_t& interval,
                                    const uint64_t& count) {
  return 0;
}
