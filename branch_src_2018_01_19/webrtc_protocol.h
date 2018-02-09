#ifndef __WEBRTC_PROTOCOL_H__
#define __WEBRTC_PROTOCOL_H__

#include <stdint.h>

#include <string>

#include "openssl/ssl.h"
#include "srtp2/srtp.h"

#include "bit_buffer.h"
#include "media_input.h"

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

    MediaInput media_input_;
};

#endif // __WEBRTC_PROTOCOL_H__
