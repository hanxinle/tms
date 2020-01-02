#ifndef __WEBRTC_PROTOCOL_H__
#define __WEBRTC_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "openssl/ssl.h"
#include "srtp2/srtp.h"

#include "bit_buffer.h"
#include "media_publisher.h"
#include "media_subscriber.h"
#include "ref_ptr.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"

class IoLoop;
class Fd;
class IoBuffer;
class WebrtcMgr;
class UdpSocket;

using std::string;

enum SctpChunkType
{
    SCTP_TYPE_DATA = 0,
    SCTP_TYPE_INIT = 1,
    SCTP_TYPE_INIT_ACK = 2,
    SCTP_TYPE_SACK = 3,
    SCTP_TYPE_HEARTBEAT = 4,
    SCTP_TYPE_HEARTBEAT_ACK = 5,
    SCTP_TYPE_ABORT = 6,
    SCTP_TYPE_SHUTDOWN = 7,
    SCTP_TYPE_SHUTDOWN_ACK = 8,
    SCTP_TYPE_ERROR = 9,
    SCTP_TYPE_COOKIE_ECHO = 10,
    SCTP_TYPE_COOKIE_ACK = 11,
    SCTP_TYPE_ECNE = 12,
    SCTP_TYPE_CWR = 13,
    SCTP_TYPE_SHUTDOWN_COMPLETE = 14,
};

enum DataChannelPPID
{
    DataChannelPPID_CONTROL = 50,
    DataChannelPPID_STRING = 51,
    DataChannelPPID_BINARY = 53,
    DataChannelPPID_STRING_EMPTY = 56,
    DataChannelPPID_BINARY_EMPTY = 57,
};

enum DataChannelMsgType
{
    DataChannelMsgType_ACK = 2,
    DataChannelMsgType_OPEN = 3,
};

enum RtcpPayloadType
{
    kSenderReport = 200,
    kReceiverReport = 201,
    kSourceDescription = 202,
    kBye = 203,
    kApp = 204,
    kRtpFeedback = 205,
    kPayloadSpecialFeedback = 206,
};

enum class WebrtcStatType
{
    kStun = 0,
    kDtls = 1,
    kRtp = 2,
    kRtcp = 3,
    kOther = 4,
};

struct SctpSession
{
    SctpSession()
        :
        src_port(0),
        dst_port(0),
        verification_tag(0),
	    checksum(0),
        chunk_type(0),
        chunk_flag(0),
        chunk_length(0),
        initiate_tag(0),
        a_rwnd(0),
        number_of_outbound_streams(0),
        number_of_inbound_streams(0),
        initial_tsn(0),
        remote_tsn(0),
        local_tsn(0),
        stream_id_s(0),
        stream_seq_num_n(0)
    {
    }

    uint32_t GetAndAddTsn()
    {
        uint32_t ret = local_tsn;
        ++local_tsn;
        return ret;
    }

    uint16_t src_port;
    uint16_t dst_port;
    uint32_t verification_tag;
	uint32_t checksum; 
    uint8_t chunk_type;
    uint8_t chunk_flag;
    uint16_t chunk_length; 

    uint32_t initiate_tag;
    uint32_t a_rwnd;
    uint16_t number_of_outbound_streams;
    uint16_t number_of_inbound_streams;
    uint32_t initial_tsn;
    // data
    uint32_t remote_tsn;
    uint32_t local_tsn;
    uint16_t stream_id_s;
    uint16_t stream_seq_num_n;
};

enum class MediaSliceCodecType
{
    kUnknown = -1,
    kVp8 = 0,
    kVp9 = 1,
    kH264 = 2,
};

enum class MediaSliceFrameType
{
    kUnknown = -1,
    kKeyFrame= 0,
    kOtherFrame = 1,
};

struct MediaSlice
{
    MediaSlice()
        :
        codec_type(MediaSliceCodecType::kUnknown),
        frame_type(MediaSliceFrameType::kUnknown),
        seq_number(-1),
        picture_id(-1),
        timestamp(-1),
        payload_length(0)
    {
    }

    MediaSliceCodecType     codec_type;
    MediaSliceFrameType     frame_type;
    int64_t                 seq_number;
    int64_t                 picture_id;
    int64_t                 timestamp;
    uint8_t                 payload[1500];
    int                     payload_length;
};

class WebrtcProtocol : public MediaPublisher, public MediaSubscriber
{
public:
    WebrtcProtocol(IoLoop* io_loop, Fd* socket);
    ~WebrtcProtocol();

    int Parse(IoBuffer& io_buffer);
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);
    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    void SetConnectState();
    void SetAcceptState();
    void SendBindingRequest();
    void SendBindingIndication();

    void SetLocalUfrag(const string& ufrag)
    {
        local_ufrag_ = ufrag;
    }

    void SetLocalPwd(const string& pwd)
    {
        local_pwd_ = pwd;
    }

    void SetRemoteUfrag(const string& ufrag)
    {
        remote_ufrag_ = ufrag;
    }

    void SetRemotePwd(const string& pwd)
    {
        remote_pwd_ = pwd;
    }

    void SendVideoData(const uint8_t* data, const int& size, const uint32_t& timestamp, const int& flag);
    void SendAudioData(const uint8_t* data, const int& size, const uint32_t& timestamp, const int& flag);

    int ProtectRtp(const uint8_t* un_protect_rtp, const int& un_protect_rtp_len, uint8_t* protect_rtp, int& protect_rtp_len);
    int UnProtectRtp(const uint8_t* protect_rtp, const int& protect_rtp_len, uint8_t* un_protect_rtp, int& un_protect_rtp_len);

    int ProtectRtcp(const uint8_t* un_protect_rtp, const int& un_protect_rtp_len, uint8_t* protect_rtp, int& protect_rtp_len);
    int UnProtectRtcp(const uint8_t* protect_rtp, const int& protect_rtp_len, uint8_t* un_protect_rtp, int& un_protect_rtp_len);

    bool DtlsHandshakeDone()
    {
        return dtls_handshake_done_;
    }

    UdpSocket* GetUdpSocket()
    {   
        return (UdpSocket*)socket_;
    }

    int DtlsSend(const uint8_t* data, const int& size);
    int SendSctpData(const uint8_t* data, const int& len, const int& type);

    void SendH264Data(const uint8_t* frame_data, const int& frame_len, const uint32_t& dts);

    bool CheckCanClose();

private:
    int OnStun(const uint8_t* data, const size_t& len);
    int OnDtls(const uint8_t* data, const size_t& len);
    int OnRtpRtcp(const uint8_t* data, const size_t& len);
    int OnSctp(const uint8_t* data, const size_t& len);

    int Handshake();

    void UpdateRecvTime(const WebrtcStatType& type, const uint64_t& time_ms)
    {
        recv_time_ms_[(int)type] = time_ms;
    }

    void AddPeriodPacketRecv(const WebrtcStatType& type, const int& count)
    {
        period_packet_recv_map_[(int)type] += count;
    }

    void AddAllPacketRecv(const WebrtcStatType& type, const int& count)
    {
        all_packet_recv_map_[(int)type] += count;
    }

private:
    IoLoop* io_loop_;
    Fd* socket_;

    uint64_t create_time_ms_;

    // key:
    map<int, uint64_t> recv_time_ms_;
    map<int, uint64_t> all_packet_recv_map_;
    map<int, uint64_t> period_packet_recv_map_;

    bool dtls_hello_send_;
    SSL* dtls_;
	BIO* bio_in_;
    BIO* bio_out_;
    bool dtls_handshake_done_;

    string client_key_;
    string server_key_;

    srtp_t srtp_send_;
    srtp_t srtp_recv_;

    string local_ufrag_;
    string local_pwd_;
    
    string remote_ufrag_;
    string remote_pwd_;

    uint64_t timestamp_base_;
    uint64_t timestamp_;

    uint32_t media_input_open_count_;
    uint64_t media_input_read_video_frame_count;

    map<uint16_t, webrtc::RtpDepacketizer::ParsedPayload> video_rtp_queue_;
    map<uint16_t, webrtc::RtpDepacketizer::ParsedPayload> audio_rtp_queue_;

    webrtc::RtpDepacketizerVp8 vp8_depacket_;
    webrtc::RtpDepacketizerVp9 vp9_depacket_;
    webrtc::RtpDepacketizerH264 h264_depacket_;

    uint32_t video_publisher_ssrc_;
    uint32_t audio_publisher_ssrc_;

    uint64_t send_begin_time_;

    SctpSession sctp_session_;
    bool datachannel_open_;

    map<uint32_t, MediaSlice> media_slice_map_;
    map<uint32_t, string> send_map_;

    uint32_t video_seq_;

    uint64_t pre_recv_data_time_ms_;
};

#endif // __WEBRTC_PROTOCOL_H__
