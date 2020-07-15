#include <map>

#include "common_define.h"
#include "http_sender.h"
#include "util.h"

std::map<std::string, std::string> kStatusMap = {
    {"200", "OK"}, {"403", "Forbidden"}, {"404", "Not Found"},
};

std::map<std::string, std::string> kTypeMap = {
    {"flv", "flv-application/octet-stream"},
    {"m3u8", "application/x-mpegurl"},
    {"ts", "video/mp2t"},
    {"html", "text/html"},
    {"js", "text/javascript"},
};

/*
    HTTP/1.0 403 Forbidden
    Server: Cdn Cache Server V2.0
    Date: Thu, 01 Feb 2018 15:15:08 GMT
    Content-Type: text/html
    Content-Length: 1612
    Expires: Thu, 01 Feb 2018 15:15:08 GMT
    X-Cache-Error: ERR_ACCESS_DENIED 0
    X-Via: 1.0 PSshwt3pj156:5 (Cdn Cache Server V2.0)[0 403 3]
    X-Ws-Request-Id: 5a732efc_PSshwt3pj156_20298-12367
    Connection: close
 * */

HttpSender::HttpSender() {
  header_kv_.push_back(std::make_pair("Server", "Trs"));
  header_kv_.push_back(std::make_pair("Date", Util::GetNowStrHttpFormat()));
}

HttpSender::~HttpSender() {}

int HttpSender::SetStatus(const std::string& status) {
  status_ = "HTTP/1.1 " + status + " " + kStatusMap[status];

  return 0;
}

int HttpSender::SetHeader(const std::string& key, const std::string& val) {
  header_kv_.push_back(make_pair(key, val));

  return 0;
}

std::string HttpSender::Encode() {
  std::ostringstream os;

  os << status_ << CRLF;

  for (const auto& header : header_kv_) {
    os << header.first << ": " << header.second << CRLF;
  }

  if (!content_.empty()) {
    os << "Content-Length: " << content_.size() << CRLF;
    os << CRLF;
    os << content_;
  } else {
    os << CRLF;
  }

  std::cout << Util::Bin2Hex(os.str()) << std::endl;

  return os.str();
}

void HttpSender::SetKeepAlive() { SetHeader("Connection", "keep-alive"); }

void HttpSender::SetClose() { SetHeader("Connection", "close"); }

void HttpSender::SetContentType(const std::string& type) {
  SetHeader("Content-Type", kTypeMap[type]);
}
