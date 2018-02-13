#ifndef __WEBRTC_PROTOCOL_H__
#define __WEBRTC_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "openssl/ssl.h"
#include "srtp2/srtp.h"

#include "bit_buffer.h"
#include "media_input.h"
#include "ref_ptr.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp9.h"

class Epoller;
class Fd;
class IoBuffer;
class WebrtcMgr;
class UdpSocket;

using std::string;

class WebrtcProtocol
{
public:
    WebrtcProtocol(Epoller* epoller, Fd* socket);
    ~WebrtcProtocol();

    int Parse(IoBuffer& io_buffer);
    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);
    int EveryNMillSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    void SendClientHello();
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

private:
    int OnStun(const uint8_t* data, const size_t& len);
    int OnDtls(const uint8_t* data, const size_t& len);
    int OnRtpRtcp(const uint8_t* data, const size_t& len);

    int Handshake();

private:
    UdpSocket* GetUdpSocket()
    {   
        return (UdpSocket*)socket_;
    }

private:
    Epoller* epoller_;
    Fd* socket_;

    bool dtls_hello_send_;
    SSL* dtls_;
	BIO* bio_in_;
    BIO* bio_out_;
    bool dtls_handshake_done_;

    string client_key_;
    string server_key_;

    srtp_t srtp_send_;
    srtp_t srtp_recv_;

    MediaInput* media_input_;

    string local_ufrag_;
    string local_pwd_;
    
    string remote_ufrag_;
    string remote_pwd_;

    uint64_t timestamp_base_;
    uint64_t timestamp_;

    uint32_t media_input_open_count_;

    map<uint16_t, webrtc::RtpDepacketizer::ParsedPayload> video_rtp_queue_;
    map<uint16_t, webrtc::RtpDepacketizer::ParsedPayload> audio_rtp_queue_;

    webrtc::RtpDepacketizerVp8 vp8_depacket_;
    webrtc::RtpDepacketizerVp9 vp9_depacket_;

    uint32_t video_publisher_ssrc_;
};

#endif // __WEBRTC_PROTOCOL_H__
