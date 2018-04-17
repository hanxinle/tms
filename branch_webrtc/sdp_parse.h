#ifndef __SDP_PARSE_H__
#define __SDP_PARSE_H__

#include <set>
#include <string>

using std::set;
using std::string;

class SdpParse
{
public:
    SdpParse();
    ~SdpParse();

    int Parse(const string& sdp);

private:
    int OnAttribute(const string& line);
    int OnMediaDesc(const string& line);
    int OnConnectionData(const string& line);

private:
    string cur_media_desc_;

    set<int> audio_support_;
    set<int> video_support_;

    string ice_ufrag_;
    string ice_pwd_;

    uint32_t audio_ssrc_;
    uint32_t video_ssrc_;
};

#endif // __SDP_PARSE_H__
