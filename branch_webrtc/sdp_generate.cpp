#include <iostream>

#include "common_define.h"
#include "sdp_generate.h"

using namespace std;

map<int, string> SdpGenerate::audio_name_ = {{111, "opus"}};
map<int, string> SdpGenerate::video_name_ = {{96, "VP8"}, {98, "VP9"}, {100, "H264"}};

string SdpGenerate::Generate()
{
    ostringstream os;

    os << "v=0" << CRLF;
    os << "o=- " << Util::GetNowMs() << " 2 IN IP4 " << server_ip_ << CRLF;
    os << "s=-" << CRLF;
    os << "t=0 0" << CRLF;
    os << "a=group:BUNDLE";

    if (! audio_support_.empty())
    {
        os << " audio";
    }

    if (! video_support_.empty())
    {
        os << " video";
    }
    os << CRLF;

    os << "a=msid-semantic: WMS " << msid_ << CRLF;

    if (! audio_support_.empty())
    {
        os << "m=audio 9 UDP/TLS/RTP/SAVPF";
        for (const auto& audio : audio_support_)
        {
            os << " " << audio;
        }

        os << CRLF;
        os << "c=IN IP4 " << server_ip_ << CRLF;
        os << "a=rtcp:9 IN IP4 " << server_ip_ << CRLF;
        os << "a=candidate:10 1 udp 2115783679 " << server_ip_ << " " << server_port_ << " typ host generation 0" << CRLF;
        os << "a=ice-ufrag:" << ice_ufrag_ << CRLF;
        os << "a=ice-pwd:" << ice_pwd_ << CRLF;
        os << "a=ice-options:trickle" << CRLF;
        os << "a=fingerprint:sha-256 " << fingerprint_ << CRLF;
        //os << "a=setup:actpass" << CRLF;
        os << "a=mid:audio" << CRLF;
        os << "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level" << CRLF;
        os << "a=" << type_ << CRLF;
        os << "a=rtcp-mux" << CRLF;

        for (const auto& audio : audio_support_)
        {
            os << "a=rtpmap:" << audio << " " << audio_name_[audio] << "/48000/2" << CRLF;
            os << "a=rtcp-fb:" << audio << " transport-cc" << CRLF;
            os << "a=fmtp:" << audio << " minptime=10;useinbandfec=1" << CRLF;
        }

        os << "a=ssrc:" << audio_ssrc_ << " cname:" << cname_ << CRLF;
        os << "a=ssrc:" << audio_ssrc_ << " msid:" << msid_ << " " << label_ << CRLF;
        os << "a=ssrc:" << audio_ssrc_ << " mslabel:" << msid_ << CRLF;
        os << "a=ssrc:" << audio_ssrc_ << " label:" << label_ << CRLF;
    }

    if (! video_support_.empty())
    {
        os << "m=video 9 UDP/TLS/RTP/SAVPF";
        for (const auto& video : video_support_)
        {
            os << " " << video;
        }

        os << CRLF;
        os << "c=IN IP4 " << server_ip_ << CRLF;
        os << "a=rtcp:9 IN IP4 " << server_ip_ << CRLF;
        os << "a=candidate:10 1 udp 2115783679 " << server_ip_ << " " << server_port_ << " typ host generation 0" << CRLF;
        os << "a=ice-ufrag:" << ice_ufrag_ << CRLF;
        os << "a=ice-pwd:" << ice_pwd_ << CRLF;
        os << "a=ice-options:trickle" << CRLF;
        os << "a=fingerprint:sha-256 " << fingerprint_ << CRLF;
        //os << "a=setup:actpass" << CRLF;
        os << "a=mid:video" << CRLF;
        os << "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset" << CRLF;
        os << "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time" << CRLF;
        os << "a=extmap:4 urn:3gpp:video-orientation" << CRLF;
        os << "a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01" << CRLF;
        os << "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay" << CRLF;
        os << "a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type" << CRLF;
        os << "a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/video-timing" << CRLF;
        os << "a=" << type_ << CRLF;
        os << "a=rtcp-mux" << CRLF;
        os << "a=rtcp-rsize" << CRLF;

        for (const auto& video : video_support_)
        {
            os << "a=rtpmap:" << video << " " << video_name_[video] << "/90000" << CRLF;
            os << "a=rtcp-fb:" << video << " goog-remb" << CRLF;
            os << "a=rtcp-fb:" << video << " transport-cc" << CRLF;
            os << "a=rtcp-fb:" << video << " ccm fir" << CRLF;
            os << "a=rtcp-fb:" << video << " nack" << CRLF;
            os << "a=rtcp-fb:" << video << " nack pli" << CRLF;

            if (video_name_[video] == "H264")
            {
                os << "a=fmtp:" << video << " level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f" << CRLF;
            }
        }

        os << "a=ssrc:" << video_ssrc_ << " cname:" << cname_ << CRLF;
        os << "a=ssrc:" << video_ssrc_ << " msid:" << msid_ << " " << label_ << CRLF;
        os << "a=ssrc:" << video_ssrc_ << " mslabel:" << msid_ << CRLF;
        os << "a=ssrc:" << video_ssrc_ << " label:" << label_ << CRLF;
    }

    cout << LMSG << "sdp generate" << endl;
    cout << os.str() << endl;

    return os.str();
}

