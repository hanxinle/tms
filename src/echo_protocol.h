#ifndef __ECHO_PROTOCOL_H__
#define __ECHO_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "socket_handler.h"

class IoLoop;
class Fd;
class IoBuffer;
class TcpSocket;

using std::string;

class EchoProtocol : public SocketHandler {
 public:
  EchoProtocol(IoLoop* io_loop, Fd* fd);
  ~EchoProtocol();

  virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
  virtual int HandleClose(IoBuffer& io_buffer, Fd& socket) { return kSuccess; }
  virtual int HandleError(IoBuffer& io_buffer, Fd& socket) {
    return HandleClose(io_buffer, socket);
  }

  int Parse(IoBuffer& io_buffer);
  int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval,
                   const uint64_t& count);

 private:
  TcpSocket* GetTcpSocket() { return (TcpSocket*)fd_; }

 private:
  IoLoop* io_loop_;
  Fd* fd_;
};

#endif  // __ECHO_PROTOCOL_H__
