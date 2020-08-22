#include "echo_protocol.h"

#include <iostream>
#include <map>

#include "common_define.h"
#include "io_buffer.h"
#include "tcp_socket.h"

EchoProtocol::EchoProtocol(IoLoop* io_loop, Fd* fd)
    : io_loop_(io_loop), fd_(fd) {}

EchoProtocol::~EchoProtocol() {}

int EchoProtocol::HandleRead(IoBuffer& io_buffer, Fd& socket) {
  return Parse(io_buffer);
}

int EchoProtocol::Parse(IoBuffer& io_buffer) {
  std::cout << LMSG << std::endl;

  uint8_t* data = NULL;
  int len = io_buffer.Read(data, io_buffer.Size());

  if (len > 0) {
    std::cout << LMSG << Util::Bin2Hex(data, len) << std::endl;
    GetTcpSocket()->Send(data, len);
  }

  return kSuccess;
}

int EchoProtocol::EveryNSecond(const uint64_t& now_in_ms,
                               const uint32_t& interval,
                               const uint64_t& count) {
  return kSuccess;
}
