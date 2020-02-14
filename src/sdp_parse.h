#ifndef __SDP_PARSE_H__
#define __SDP_PARSE_H__

#include <set>
#include <string>

class SdpParse
{
public:
    SdpParse();
    ~SdpParse();

    int Parse(const std::string& sdp);

private:
    int OnAttribute(const std::string& line);
    int OnMediaDesc(const std::string& line);
    int OnConnectionData(const std::string& line);

private:
    std::string cur_media_desc_;

    std::set<int> audio_support_;
    std::set<int> video_support_;

    std::string ice_ufrag_;
    std::string ice_pwd_;

    uint32_t audio_ssrc_;
    uint32_t video_ssrc_;
};

#endif // __SDP_PARSE_H__
