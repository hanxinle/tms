#ifndef __SDP_GENERATE_H__
#define __SDP_GENERATE_H__

#include <set>
#include <string>

using std::set;
using std::string;

class SdpGenerate
{
public:
    SdpGenerate() {}
    ~SdpGenerate() {}

    void SetType(const string& type) { type_ = type; }
    void SetServerIp(const string& server_ip) { server_ip_ = server_ip; }
    void SetServerPort(const uint16_t& server_port) { server_port_ = server_port; }
    void SetMsid(const string& msid) { msid_ = msid; }
    void SetCname(const string& cname) { cname_ = cname; }
    void SetLabel(const string& label) { label_ = label; }
    void SetIceUfrag(const string& ufrag) { ice_ufrag_ = ufrag; }
    void SetIcePwd(const string& pwd) { ice_pwd_ = pwd; }
    void SetFingerprint(const string& fingerprint) { fingerprint_ = fingerprint; }
    void AddAudioSupport(const int& support) { audio_support_.insert(support); }
    void AddVideoSupport(const int& support) { video_support_.insert(support); }
    void SetAudioSsrc(const uint64_t& ssrc) { audio_ssrc_ = ssrc; }
    void SetVideoSsrc(const uint64_t& ssrc) { video_ssrc_ = ssrc; }

    string Generate();

private:
    string type_;
    string server_ip_;
    uint16_t server_port_;
    string msid_;
    string cname_;
    string label_;
    string ice_ufrag_;
    string ice_pwd_;
    string fingerprint_;
    set<int> audio_support_;
    set<int> video_support_;

    uint64_t audio_ssrc_;
    uint64_t video_ssrc_;

private:
    static map<int, string> audio_name_;
    static map<int, string> video_name_;
};

#endif // __SDP_GENERATE_H__
