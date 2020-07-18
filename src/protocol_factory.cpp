#include "protocol_factory.h"
#include "http_dash_protocol.h"
#include "http_file_protocol.h"
#include "http_flv_protocol.h"
#include "http_hls_protocol.h"
#include "rtmp_protocol.h"
#include "srt_protocol.h"
#include "web_socket_protocol.h"
#include "webrtc_protocol.h"

SocketHandler* ProtocolFactory::GenRtmpProtocol(IoLoop* io_loop, Fd* fd) {
  return new RtmpProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenHttpFlvProtocol(IoLoop* io_loop, Fd* fd) {
  return new HttpFlvProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenHttpHlsProtocol(IoLoop* io_loop, Fd* fd) {
  return new HttpHlsProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenHttpDashProtocol(IoLoop* io_loop, Fd* fd) {
  return new HttpDashProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenHttpFileProtocol(IoLoop* io_loop, Fd* fd) {
  return new HttpFileProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenWebSocketProtocol(IoLoop* io_loop, Fd* fd) {
  return new WebSocketProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenSrtProtocol(IoLoop* io_loop, Fd* fd) {
  return new SrtProtocol(io_loop, fd);
}

SocketHandler* ProtocolFactory::GenWebrtcProtocol(IoLoop* io_loop, Fd* fd) {
  return new WebrtcProtocol(io_loop, fd);
}
