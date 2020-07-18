#include <iostream>
#include <map>

#include "common_define.h"
#include "global.h"
#include "http_dash_protocol.h"
#include "io_buffer.h"
#include "local_stream_center.h"
#include "rtmp_protocol.h"
#include "tcp_socket.h"
#include "util.h"

HttpDashProtocol::HttpDashProtocol(IoLoop* io_loop, Fd* socket)
    : MediaSubscriber(kHttpHls),
      io_loop_(io_loop),
      socket_(socket),
      media_publisher_(NULL) {}

HttpDashProtocol::~HttpDashProtocol() {}

int HttpDashProtocol::HandleRead(IoBuffer& io_buffer, Fd& socket) {
  int ret = kError;
  do {
    ret = Parse(io_buffer);
  } while (ret == kSuccess);

  return ret;
}

int HttpDashProtocol::Parse(IoBuffer& io_buffer) {
  uint8_t* data = NULL;

  int size = io_buffer.Read(data, io_buffer.Size());

  int r_pos = -1;  // '\r'
  int n_pos = -1;  // '\n'
  int m_pos = -1;  // ':'

  bool key_value = false;  // false:key, true:value

  std::string key;
  std::string value;

  std::map<std::string, std::string> header;

  app_.clear();
  stream_.clear();
  segment_.clear();
  type_.clear();

  for (int i = 0; i != size; ++i) {
    if (data[i] == '\r') {
      r_pos = i;
    } else if (data[i] == '\n') {
      if (i == r_pos + 1) {
        if (i == n_pos + 2)  // \r\n\r\n
        {
          std::cout << LMSG << "http done" << std::endl;

          std::cout << LMSG << "app_:" << app_ << ",stream_:" << stream_
                    << ",segment_:" << segment_ << ",type_:" << type_
                    << std::endl;
          if (!app_.empty() && !stream_.empty()) {
            media_publisher_ =
                g_local_stream_center.GetMediaPublisherByAppStream(app_,
                                                                   stream_);

            if (media_publisher_ != NULL) {
              if (type_ == "m4s") {
                std::vector<std::string> tmp = Util::SepStr(segment_, "_");
                if (tmp.size() != 2 ||
                    (tmp[0].find("audio") == std::string::npos &&
                     tmp[0].find("video") == std::string::npos)) {
                  std::ostringstream os;

                  os << "HTTP/1.1 404 Not Found\r\n"
                     << "Server: tms\r\n"
                     << "Connection: close\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  return kClose;
                }

                PayloadType payload_type =
                    tmp[0].find("video") != std::string::npos ? kVideoPayload
                                                              : kAudioPayload;
                uint64_t seg_num = Util::Str2Num<uint64_t>(tmp[1]);

                const std::string& m4s =
                    media_publisher_->GetDashMuxer().GetM4s(payload_type,
                                                            seg_num);

                if (!m4s.empty()) {
                  std::ostringstream os;

                  os << "HTTP/1.1 200 OK\r\n"
                     << "Server: tms\r\n"
                     << "Content-Type: text/plain\r\n"
                     << "Connection: keep-alive\r\n"
                     << "Content-Length:" << m4s.size() << "\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  GetTcpSocket()->Send((const uint8_t*)m4s.data(), m4s.size());
                } else {
                  std::ostringstream os;

                  os << "HTTP/1.1 404 Not Found\r\n"
                     << "Server: tms\r\n"
                     << "Connection: close\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  return kClose;
                }
              } else if (type_ == "mp4") {
                if (segment_.find("audio") == std::string::npos &&
                    segment_.find("video") == std::string::npos) {
                  std::ostringstream os;

                  os << "HTTP/1.1 404 Not Found\r\n"
                     << "Server: tms\r\n"
                     << "Connection: close\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  return kClose;
                }

                PayloadType payload_type =
                    segment_.find("video") != std::string::npos ? kVideoPayload
                                                                : kAudioPayload;
                const std::string& init_mp4 =
                    media_publisher_->GetDashMuxer().GetInitMp4(payload_type);

                if (!init_mp4.empty()) {
                  std::ostringstream os;

                  os << "HTTP/1.1 200 OK\r\n"
                     << "Server: tms\r\n"
                     << "Content-Type: video/mp4\r\n"
                     << "Connection: keep-alive\r\n"
                     << "Content-Length:" << init_mp4.size() << "\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  GetTcpSocket()->Send((const uint8_t*)init_mp4.data(),
                                       init_mp4.size());
                } else {
                  std::ostringstream os;

                  os << "HTTP/1.1 404 Not Found\r\n"
                     << "Server: tms\r\n"
                     << "Connection: close\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  return kClose;
                }
              } else if (type_ == "mpd") {
                std::string mpd = media_publisher_->GetDashMuxer().GetMpd();

                if (!mpd.empty()) {
                  std::ostringstream os;

                  Util::Replace(mpd, "${app}/${stream}", app_ + "/" + stream_);

                  os << "HTTP/1.1 200 OK\r\n"
                     << "Server: tms\r\n"
                     << "Content-Type: text/plain\r\n"
                     << "Connection: keep-alive\r\n"
                     << "Content-Length:" << mpd.size() << "\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  GetTcpSocket()->Send((const uint8_t*)mpd.data(), mpd.size());
                } else {
                  std::ostringstream os;

                  os << "HTTP/1.1 404 Not Found\r\n"
                     << "Server: tms\r\n"
                     << "Connection: close\r\n"
                     << "\r\n";

                  GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                       os.str().size());
                  return kClose;
                }
              }
            } else {
              std::cout << LMSG << "can't find media source, app_:" << app_
                        << ",stream_:" << stream_ << std::endl;

              std::ostringstream os;

              os << "HTTP/1.1 404 Not Found\r\n"
                 << "Server: tms\r\n"
                 << "Connection: close\r\n"
                 << "\r\n";

              GetTcpSocket()->Send((const uint8_t*)os.str().data(),
                                   os.str().size());
              return kClose;
            }
          }

          return kSuccess;
        } else  // \r\n
        {
          key_value = false;

          std::cout << LMSG << key << ":" << value << std::endl;

          if (key.find("GET") != std::string::npos) {
            // GET /test.flv HTTP/1.1
            int s_count = 0;
            int x_count = 0;  // /
            int d_count = 0;  // .
            for (const auto& ch : key) {
              if (ch == ' ') {
                ++s_count;
              } else if (ch == '/') {
                ++x_count;
              } else if (ch == '.') {
                ++d_count;
              } else {
                if (x_count == 1) {
                  app_ += ch;
                } else if (x_count == 2) {
                  stream_ += ch;
                } else if (x_count == 3 && s_count < 2) {
                  if (d_count == 1) {
                    type_ += ch;
                  } else {
                    segment_ += ch;
                  }
                }
              }
            }
          }

          header[key] = value;
          key.clear();
          value.clear();
        }
      }

      n_pos = i;
    } else if (data[i] == ':') {
      m_pos = i;
      key_value = true;
    } else if (data[i] == ' ') {
      if (m_pos + 1 == i) {
      } else {
        if (key_value) {
          value += (char)data[i];
        } else {
          key += (char)data[i];
        }
      }
    } else {
      if (key_value) {
        value += (char)data[i];
      } else {
        key += (char)data[i];
      }
    }
  }

  return kNoEnoughData;
}
