#ifndef __SDP_GENERATE_H__
#define __SDP_GENERATE_H__

#include <set>
#include <string>

class SdpGenerate
{
public:
    SdpGenerate() {}
    ~SdpGenerate() {}

    void SetType(const std::string& type) { type_ = type; }
    void SetServerIp(const std::string& server_ip) { server_ip_ = server_ip; }
    void SetServerPort(const uint16_t& server_port) { server_port_ = server_port; }
    void SetMsid(const std::string& msid) { msid_ = msid; }
    void SetCname(const std::string& cname) { cname_ = cname; }
    void SetLabel(const std::string& label) { label_ = label; }
    void SetIceUfrag(const std::string& ufrag) { ice_ufrag_ = ufrag; }
    void SetIcePwd(const std::string& pwd) { ice_pwd_ = pwd; }
    void SetFingerprint(const std::string& fingerprint) { fingerprint_ = fingerprint; }
    void AddAudioSupport(const int& support) { audio_support_.insert(support); }
    void AddVideoSupport(const int& support) { video_support_.insert(support); }
    void SetAudioSsrc(const uint64_t& ssrc) { audio_ssrc_ = ssrc; }
    void SetVideoSsrc(const uint64_t& ssrc) { video_ssrc_ = ssrc; }

    std::string Generate();

private:
    std::string type_;
    std::string server_ip_;
    uint16_t server_port_;
    std::string msid_;
    std::string cname_;
    std::string label_;
    std::string ice_ufrag_;
    std::string ice_pwd_;
    std::string fingerprint_;
    std::set<int> audio_support_;
    std::set<int> video_support_;

    uint64_t audio_ssrc_;
    uint64_t video_ssrc_;

private:
    static std::map<int, std::string> audio_name_;
    static std::map<int, std::string> video_name_;
};

#endif // __SDP_GENERATE_H__
