#include <iostream>
#include <map>

#include "base_64.h"
#include "bit_buffer.h"
#include "common_define.h"
#include "global.h"
#include "http_file_protocol.h"
#include "http_sender.h"
#include "io_buffer.h"
#include "tcp_socket.h"

HttpFileProtocol::HttpFileProtocol(IoLoop* io_loop, Fd* socket)
    : io_loop_(io_loop), socket_(socket), upgrade_(false) {}

HttpFileProtocol::~HttpFileProtocol() {}

int HttpFileProtocol::HandleRead(IoBuffer& io_buffer, Fd& socket) {
  int ret = kError;
  do {
    ret = Parse(io_buffer);
  } while (ret == kSuccess);

  return ret;
}

int HttpFileProtocol::Parse(IoBuffer& io_buffer) {
  int ret = http_parse_.Decode(io_buffer);

  if (ret == kSuccess) {
    if (http_parse_.GetFileType() == "html") {
      std::string html = Util::ReadFile(http_parse_.GetFileName() + ".html");
      if (html.empty()) {
        HttpSender http_rsp;
        http_rsp.SetStatus("404");
        http_rsp.SetContentType("html");
        http_rsp.SetClose();
        http_rsp.SetContent("no found");

        std::string http_response = http_rsp.Encode();

        GetTcpSocket()->Send((const uint8_t*)http_response.data(),
                             http_response.size());
      } else {
        HttpSender http_rsp;
        http_rsp.SetStatus("200");
        http_rsp.SetContentType("html");
        http_rsp.SetClose();
        http_rsp.SetContent(html);

        std::string http_response = http_rsp.Encode();

        GetTcpSocket()->Send((const uint8_t*)http_response.data(),
                             http_response.size());
      }
    } else {
      HttpSender http_rsp;
      http_rsp.SetStatus("404");
      http_rsp.SetContentType("html");
      http_rsp.SetClose();
      http_rsp.SetContent("no found");

      std::string http_response = http_rsp.Encode();

      GetTcpSocket()->Send((const uint8_t*)http_response.data(),
                           http_response.size());
    }
  }

  return ret;
}

int HttpFileProtocol::Send(const uint8_t* data, const size_t& len) {
  return kSuccess;
}

int HttpFileProtocol::EveryNSecond(const uint64_t& now_in_ms,
                                   const uint32_t& interval,
                                   const uint64_t& count) {
  return 0;
}
