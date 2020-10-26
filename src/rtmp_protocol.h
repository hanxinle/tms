#ifndef __RTMP_PROTOCOL_H__
#define __RTMP_PROTOCOL_H__

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <sstream>

#include "crc32.h"
#include "media_publisher.h"
#include "media_subscriber.h"
#include "ref_ptr.h"
#include "socket_handler.h"
#include "socket_util.h"

class AmfCommand;
class IoLoop;
class Fd;
class IoBuffer;
class TcpSocket;

enum HandShakeStatus {
  kStatus_0 = 0,
  kStatus_1,
  kStatus_2,
  kStatus_Done,
};

enum RtmpMessageType {
  kSetChunkSize = 1,
  kAcknowledgement = 3,
  kUserControlMessage = 4,
  kWindowAcknowledgementSize = 5,
  kSetPeerBandwidth = 6,

  kAudio = 8,
  kVideo = 9,

  kMetaData_AMF3 = 15,
  kAmf3Command = 17,
  kMetaData_AMF0 = 18,
  kAmf0Command = 20,
};

enum class RtmpRole {
  // other_server --> me --> client

  kUnknownRtmpRole = -1,
  kClientPush = 0,
  kPushServer = 1,
  kPullServer = 2,
  kClientPull = 3,
};

struct RtmpUrl {
  std::string ip;
  uint16_t port;
  std::string app;
  std::string stream;
  std::map<std::string, std::string> args;
};

struct RtmpMessage {
  RtmpMessage()
      : cs_id(0),
        timestamp(0),
        timestamp_delta(0),
        timestamp_calc(0),
        message_length(0),
        message_type_id(0),
        message_stream_id(0),
        msg(NULL),
        len(0) {}

  RtmpMessage& operator=(const RtmpMessage& other) {
    cs_id = other.cs_id;
    timestamp = other.timestamp;
    timestamp_delta = other.timestamp_delta;
    timestamp_calc = other.timestamp_calc;
    message_length = other.message_length;
    message_type_id = other.message_type_id;
    message_stream_id = other.message_stream_id;

    msg = NULL;
    len = 0;

    return *this;
  }

  RtmpMessage(const RtmpMessage& other) { operator=(other); }

  std::string ToString() const {
    std::ostringstream os;

    os << "cs_id:" << cs_id << ",timestamp:" << timestamp
       << ",timestamp_delta:" << timestamp_delta
       << ",timestamp_calc:" << timestamp_calc
       << ",message_length:" << message_length
       << ",message_type_id:" << (uint16_t)message_type_id
       << ",message_stream_id:" << message_stream_id << ",msg:" << (uint64_t)msg
       << ",len:" << len;

    return os.str();
  }

  uint32_t cs_id;
  uint32_t timestamp;
  uint32_t timestamp_delta;
  uint32_t timestamp_calc;
  uint32_t message_length;
  uint8_t message_type_id;
  uint32_t message_stream_id;

  uint8_t* msg;
  uint32_t len;
};

class RtmpProtocol : public MediaPublisher,
                     public MediaSubscriber,
                     public SocketHandler {
 public:
  RtmpProtocol(IoLoop* io_loop, Fd* socket);
  ~RtmpProtocol();

  virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
  virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
  virtual int HandleError(IoBuffer& io_buffer, Fd& socket) {
    return HandleClose(io_buffer, socket);
  }

  virtual int HandleConnected(Fd& socket);

  bool IsServerRole() {
    return role_ == RtmpRole::kClientPull || role_ == RtmpRole::kClientPush;
  }

  bool IsClientRole() {
    return role_ == RtmpRole::kPushServer || role_ == RtmpRole::kPullServer;
  }

  void SetClientPush() { role_ = RtmpRole::kClientPush; }

  void SetPushServer() { role_ = RtmpRole::kPushServer; }

  void SetPullServer() { role_ = RtmpRole::kPullServer; }

  void SetClientPull() { role_ = RtmpRole::kClientPull; }

  void SetRole(const RtmpRole& role) { role_ = role; }

  uint32_t GetDigestOffset(const uint8_t scheme, const uint8_t* buf);
  uint32_t GetKeyOffset(const uint8_t& scheme, const uint8_t* buf);
  bool GuessScheme(const uint8_t& scheme, const uint8_t* buf);

  int Parse(IoBuffer& io_buffer);

  int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval,
                   const uint64_t& count);
  int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval,
                       const uint64_t& count) {
    return 0;
  }

  int SendHandShakeStatus0();
  int SendHandShakeStatus1();
  int SetOutChunkSize(const uint32_t& chunk_size);
  int SetWindowAcknowledgementSize(const uint32_t& ack_window_size);
  int SendBwDone();
  int SetPeerBandwidth(const uint32_t& ack_window_size,
                       const uint8_t& limit_type);
  int SendUserControlMessage(const uint16_t& event, const uint32_t& data);
  int SendConnect(const std::string& url);
  int SendCreateStream();
  int SendReleaseStream();
  int SendFCPublish();
  int SendCheckBw();
  int SendPublish(const double& stream_id);
  int SendPlay(const double& stream_id);
  int SendAudio(const RtmpMessage& audio);
  int SendVideo(const RtmpMessage& video);

  void SetApp(const std::string& app) {
    app_ = app;
    media_muxer_.SetApp(app_);
  }

  void SetStreamName(const std::string& stream) {
    stream_ = stream;
    media_muxer_.SetStreamName(stream_);
  }

  void SetDomain(const std::string& domain) { domain_ = domain; }

  void SetArgs(const std::map<std::string, std::string>& args) { args_ = args; }

  static int ParseRtmpUrl(const std::string& url, RtmpUrl& rtmp_url);

  TcpSocket* GetTcpSocket() { return (TcpSocket*)socket_; }

  bool CanPublish() { return can_publish_; }

  int SendRtmpMessage(const uint32_t cs_id, const uint32_t& message_stream_id,
                      const uint8_t& message_type_id, const uint8_t* data,
                      const size_t& len);
  int SendMediaData(const Payload& media);

  virtual int SendVideoHeader(const std::string& header);
  virtual int SendAudioHeader(const std::string& header);
  virtual int SendMetaData(const std::string& metadata);

 private:
  double GetTransactionId() {
    transaction_id_ += 1.0;

    return transaction_id_;
  }

  std::string DumpIdCommand() {
    std::ostringstream os;
    for (const auto& kv : id_command_) {
      os << kv.first << "=>" << kv.second << ",";
    }

    return os.str();
  }

  bool IsHandshakeDone() { return handshake_status_ == kStatus_Done; }

  int OnConnectCommand(AmfCommand& amf_command);
  int OnCreateStreamCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command);
  int OnPlayCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command);
  int OnPublishCommand(RtmpMessage& rtmp_msg, AmfCommand& amf_command);
  int OnResultCommand(AmfCommand& amf_command);
  int OnStatusCommand(AmfCommand& amf_command);

  int OnAmf0Message(RtmpMessage& rtmp_msg);
  int OnAudio(RtmpMessage& rtmp_msg);
  int OnSetChunkSize(RtmpMessage& rtmp_msg);
  int OnAcknowledgement(RtmpMessage& rtmp_msg);
  int OnUserControlMessage(RtmpMessage& rtmp_msg);
  int OnVideo(RtmpMessage& rtmp_msg);
  int OnWindowAcknowledgementSize(RtmpMessage& rtmp_msg);
  int OnSetPeerBandwidth(RtmpMessage& rtmp_msg);
  int OnMetaData(RtmpMessage& rtmp_msg);

  int OnVideoHeader(RtmpMessage& rtmp_msg);

  virtual int OnPendingArrive();

  int OnRtmpMessage(RtmpMessage& rtmp_msg);
  int SendData(const RtmpMessage& cur_info, const Payload& paylod = Payload(),
               const bool& force_fmt0 = false);

  void GenerateRandom(uint8_t* data, const int& len);

 private:
  IoLoop* io_loop_;
  Fd* socket_;
  HandShakeStatus handshake_status_;

  RtmpRole role_;

  // complex handshake or simple handshake
  uint8_t version_;
  uint8_t scheme_;
  bool encrypted_;

  uint32_t in_chunk_size_;
  uint32_t out_chunk_size_;

  std::map<uint32_t, RtmpMessage> csid_head_;
  std::map<uint32_t, RtmpMessage> csid_pre_info_;

  RtmpMessage pending_rtmp_msg_;

  std::string app_;
  std::string tc_url_;
  std::string stream_;
  std::string domain_;
  std::map<std::string, std::string> args_;

  double transaction_id_;

  std::string last_send_command_;

  std::map<double, std::string> id_command_;

  bool can_publish_;

  uint64_t video_frame_send_;
  uint64_t audio_frame_send_;
};

#endif  // __RTMP_PROTOCOL_H__
