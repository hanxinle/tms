#ifndef __SRT_SOCKET_UTIL_H__
#define __SRT_SOCKET_UTIL_H__

#include <string>

#include "socket_util.h"
#include "srt/srt.h"
#include "srt/udt.h"
#include "srt_socket_util.h"
#include "util.h"

namespace srt_socket_util {
inline int CreateSrtSocket() {
  int ret = srt_socket(AF_INET, SOCK_DGRAM, 0);

  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_socket failed, err=" << srt_getlasterror_str()
              << std::endl;
  }

  return ret;
}

inline int Bind(const int& fd, const std::string& ip, const uint16_t& port) {
  sockaddr_in sa;
  socket_util::IpPortToSocketAddr(ip, port, sa);
  int ret = srt_bind(fd, (sockaddr*)&sa, sizeof(sa));

  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_bind failed, err=" << srt_getlasterror_str()
              << std::endl;
  }

  return ret;
}

inline int Connect(const int& fd, const std::string& ip, const uint16_t& port) {
  sockaddr_in sa;
  socket_util::IpPortToSocketAddr(ip, port, sa);
  int ret = srt_connect(fd, (sockaddr*)&sa, sizeof(sa));

  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_connect failed, err=" << srt_getlasterror_str()
              << std::endl;
  }

  return ret;
}

inline int Listen(const int& fd) {
  int ret = srt_listen(fd, 5);

  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_listen failed, err=" << srt_getlasterror_str()
              << std::endl;
  }

  return ret;
}

inline int SetTransTypeLive(const int& fd) {
  SRT_TRANSTYPE tt = SRTT_LIVE;
  if (SRT_ERROR == srt_setsockopt(fd, 0, SRTO_TRANSTYPE, &tt, sizeof tt)) {
    std::cout << LMSG << "srt_setsockopt SRTO_TRANSTYPE failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int ReuseAddr(const int& fd) {
  bool reuse = true;
  int ret = srt_setsockopt(fd, 0, SRTO_REUSEADDR, &reuse, sizeof(reuse));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_REUSEADDR failed, err="
              << srt_getlasterror_str() << std::endl;
  }

  return ret;
}

inline int SetBlock(const int& fd, const bool& block) {
  int ret = srt_setsockopt(fd, 0, SRTO_RCVSYN, &block, sizeof(block));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_RCVSYN failed, err="
              << srt_getlasterror_str() << std::endl;
  }

  ret = srt_setsockopt(fd, 0, SRTO_SNDSYN, &block, sizeof(block));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_SNDSYN failed, err="
              << srt_getlasterror_str() << std::endl;
  }

  return ret;
}

inline int SetSendBufSize(const int& fd, const int& sndbuf) {
  int ret =
      srt_setsockopt(fd, SOL_SOCKET, SRTO_SNDBUF, &sndbuf, sizeof(sndbuf));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_SNDBUF failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int SetRecvBufSize(const int& fd, const int& rcvbuf) {
  int ret =
      srt_setsockopt(fd, SOL_SOCKET, SRTO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_RCVBUF failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int SetUdpSendBufSize(const int& fd, const int& sndbuf) {
  int ret =
      srt_setsockopt(fd, SOL_SOCKET, SRTO_UDP_SNDBUF, &sndbuf, sizeof(sndbuf));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_UDP_SNDBUF failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int SetUdpRecvBufSize(const int& fd, const int& rcvbuf) {
  int ret =
      srt_setsockopt(fd, SOL_SOCKET, SRTO_UDP_RCVBUF, &rcvbuf, sizeof(rcvbuf));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_UDP_RCVBUF failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int SetLatency(const int& fd, const int& latency) {
  int ret =
      srt_setsockopt(fd, SOL_SOCKET, SRTO_LATENCY, &latency, sizeof(latency));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_LATENCY failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int SetTspbdMode(const int& fd, const int& tsbpd) {
  int ret =
      srt_setsockopt(fd, SOL_SOCKET, SRTO_TSBPDMODE, &tsbpd, sizeof(tsbpd));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_TSBPDMODE failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

inline int SetPeerIdleTimeout(const int& fd, const int& peer_idle_timeout_ms) {
  int ret = srt_setsockopt(fd, SOL_SOCKET, SRTO_PEERIDLETIMEO,
                           &peer_idle_timeout_ms, sizeof(peer_idle_timeout_ms));
  if (ret == SRT_ERROR) {
    std::cout << LMSG << "srt_setsockopt SRTO_PEERIDLETIMEO failed, err="
              << srt_getlasterror_str() << std::endl;
    return -1;
  }

  return 0;
}

}  // namespace srt_socket_util

#endif  // __SRT_SOCKET_UTIL_H__
