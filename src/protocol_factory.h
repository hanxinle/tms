#ifndef __PROTOCOL_FACTORY_H__
#define __PROTOCOL_FACTORY_H__

class Fd;
class IoLoop;
class SocketHandler;

class ProtocolFactory {
 public:
  static SocketHandler* GenRtmpProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenHttpFlvProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenHttpHlsProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenHttpDashProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenHttpFileProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenWebSocketProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenSrtProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenWebrtcProtocol(IoLoop* io_loop, Fd* fd);
  static SocketHandler* GenEchoProtocol(IoLoop* io_loop, Fd* fd);
};

#endif  // __PROTOCOL_FACTORY_H__
