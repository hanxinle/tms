#ifndef __HTTP_SENDER_H__
#define __HTTP_SENDER_H__

#include <string>
#include <utility>
#include <vector>

class HttpSender {
 public:
  HttpSender();
  ~HttpSender();

  int SetHeader(const std::string& key, const std::string& val);
  std::string Encode();
  int SetStatus(const std::string& status);
  void SetKeepAlive();
  void SetClose();
  void SetContent(const std::string& content) { content_ = content; }

  void SetContentType(const std::string& type);

 private:
  std::string status_;
  std::vector<std::pair<std::string, std::string> > header_kv_;
  std::string content_;
};

#endif  // __HTTP_SENDER_H__
