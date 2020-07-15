#include "srt_socket.h"
#include "global.h"
#include "srt_socket_util.h"
#include "util.h"

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "srt/srt.h"

SrtSocket::SrtSocket(IoLoop* io_loop, const int& fd,
                     HandlerFactoryT handler_factory)
    : Fd(io_loop, fd),
      connect_status_(kDisconnected),
      handler_factory_(handler_factory),
      server_socket_(false),
      stream_id_("") {
  std::cout << LMSG << "fd=" << fd_ << ",srt socket=" << this << std::endl;
  socket_handler_ = handler_factory_(io_loop_, this);
}

SrtSocket::~SrtSocket() {
  std::cout << LMSG << "fd=" << fd_ << ",srt socket=" << this << std::endl;
  delete socket_handler_;
}

int SrtSocket::OnRead() {
  if (server_socket_) {
    sockaddr_in sa;
    int sa_len = sizeof(sa);

    SRTSOCKET client_srt_socket = srt_accept(fd_, (sockaddr*)&sa, &sa_len);

    if (client_srt_socket != SRT_INVALID_SOCK) {
      SrtSocket* srt_socket =
          new SrtSocket(io_loop_, client_srt_socket, handler_factory_);
      srt_socket->SetConnected();
      srt_socket->EnableRead();
      srt_socket->SetStreamId(UDT::getstreamid(client_srt_socket));

      std::string client_ip = "";
      uint16_t client_port = 0;

      socket_util::SocketAddrToIpPort(sa, client_ip, client_port);
      srt_socket_util::SetBlock(client_srt_socket, false);
      srt_socket->ModName(name() + " <-> " + client_ip + ":" +
                          Util::Num2Str(client_port));

      std::cout << LMSG << srt_socket->name()
                << " accept, fd:" << client_srt_socket
                << ", streamid:" << UDT::getstreamid(client_srt_socket)
                << std::endl;
    }

    return kSuccess;
  } else {
    SRT_SOCKSTATUS srt_status = srt_getsockstate(fd());
    if (srt_status == SRTS_CLOSED || srt_status == SRTS_BROKEN) {
      std::cout << LMSG << name() << ", srt_status=" << (int)srt_status
                << std::endl;
      socket_handler_->HandleClose(read_buffer_, *this);

      return kClose;
    }

    uint8_t buf[1024 * 8];
    int msg_count = 0;
    while (true) {
      int ret = srt_recvmsg(fd(), (char*)buf, sizeof(buf));

      if (ret == SRT_ERROR &&
          srt_getlasterror(NULL) == (MJ_AGAIN * 1000 + MN_RDAVAIL)) {
        std::cout << LMSG << "msg_count=" << msg_count
                  << ", no more data to read" << std::endl;
        break;
      }

      if (ret == SRT_ERROR) {
        std::cout << LMSG << name() << " read error" << std::endl;
        socket_handler_->HandleError(read_buffer_, *this);

        return kError;
      }

      ++msg_count;

      // std::cout << LMSG << "srt recv:" << ret << " bytes, " <<
      // Util::Bin2Hex(buf, ret) << std::endl;

      read_buffer_.Write(buf, ret);
      ret = socket_handler_->HandleRead(read_buffer_, *this);

      if (ret == kClose || ret == kError) {
        std::cout << LMSG << name() << " handle error" << std::endl;
        socket_handler_->HandleClose(read_buffer_, *this);
        return kClose;
      }
    }
  }

  return kSuccess;
}

int SrtSocket::OnWrite() {
  if (IsConnecting()) {
    std::cout << LMSG << "fd=" << fd_ << ",srt socket=" << this
              << ", connect established" << std::endl;
    SetConnected();
    DisableWrite();
    EnableRead();
  }

  return kSuccess;
}

int SrtSocket::ConnectIp(const std::string& ip, const uint16_t& port) {
  int ret = srt_socket_util::Connect(fd_, ip, port);

  if (ret < 0) {
    return ret;
  }

  SetConnecting();
  EnableWrite();

  return kSuccess;
}

int SrtSocket::ConnectHost(const std::string& host, const uint16_t& port) {
  std::string ip = socket_util::GetIpByHost(host);

  if (ip.empty()) {
    return kError;
  }

  return ConnectIp(ip, port);
}

int SrtSocket::Send(const uint8_t* data, const size_t& len) {
  srt_sendmsg(fd(), (const char*)data, len, -1, 0);

  return kSuccess;
}
