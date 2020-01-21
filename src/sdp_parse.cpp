#include <iostream>

#include "common_define.h"
#include "sdp_parse.h"
#include "util.h"

SdpParse::SdpParse()
{
}

SdpParse::~SdpParse()
{
}

int SdpParse::Parse(const std::string& sdp)
{
    std::vector<std::string> v = Util::SepStr(sdp, CRLF);

    if (v.empty())
    {
        return kError;
    }

    for (const auto& line : v)
    {
        if (line.size() > 2)
        {
            if (line[0] == 'v' && line[1] == '=')
            {
            }
            else if (line[0] == 'o' && line[1] == '=')
            {
            }
            else if (line[0] == 's' && line[1] == '=')
            {
            }
            else if (line[0] == 't' && line[1] == '=')
            {
            }
            else if (line[0] == 'a' && line[1] == '=')
            {
                OnAttribute(line.substr(2));
            }
            else if (line[0] == 'm' && line[1] == '=')
            {
                OnMediaDesc(line.substr(2));
            }
            else if (line[0] == 'c' && line[1] == '=')
            {
                OnConnectionData(line.substr(2));
            }
        }
    }

    return 0;
}

int SdpParse::OnAttribute(const std::string& line)
{
    auto pos = line.find(":");

    if (pos == std::string::npos)
    {
        pos = -1;
    }

    std::string attribute = line.substr(0, pos);

    if (attribute == "ice-ufrag")
    {
        ice_ufrag_ = line.substr(pos + 1);
        std::cout << LMSG << "ice_ufrag_:" << ice_ufrag_ << std::endl;
    }
    else if (attribute == "ice-pwd")
    {
        ice_pwd_ = line.substr(pos + 1);
        std::cout << LMSG << "ice_pwd_:" << ice_pwd_ << std::endl;
    }
    else if (attribute == "ice-options")
    {
    }
    else if (attribute == "fingerprint")
    {
    }
    else if (attribute == "setup")
    {
    }
    else if (attribute == "mid")
    {
    }
    else if (attribute == "ssrc")
    {
        std::vector<std::string> v = Util::SepStr(line.substr(pos + 1), " ");
        if (cur_media_desc_ == "audio" && v.size() > 1)
        {
            audio_ssrc_ = Util::Str2Num<uint32_t>(v[0]);
            std::cout << LMSG << "audio_ssrc_:" << audio_ssrc_ << std::endl;
        }
        else if (cur_media_desc_ == "video" && v.size() > 1)
        {
            video_ssrc_ = Util::Str2Num<uint32_t>(v[0]);
            std::cout << LMSG << "video_ssrc_:" << video_ssrc_ << std::endl;
        }
    }
    else if (attribute == "rtpmap")
    {
        std::vector<std::string> v = Util::SepStr(line.substr(pos + 1), " ");

        if (v.size() == 2)
        {
            std::vector<std::string> codec_time = Util::SepStr(v[1], "/");
            if (codec_time.size() == 2)
            {
                std::cout << LMSG << "codec:" << codec_time[0] << ", clock:" << codec_time[1] << std::endl;
            }
        }
    }
    else if (attribute == "rtcp-fb")
    {
    }
    else if (attribute == "fmtp")
    {
    }

    return 0;
}

int SdpParse::OnMediaDesc(const std::string& line)
{
    std::vector<std::string> v = Util::SepStr(line, " ");

    if (v.size() > 3)
    {
        std::string type = v[0];
        std::string trans_protocol = v[2];

        v.erase(v.begin());
        v.erase(v.begin());
        v.erase(v.begin());

        for (const auto& item : v)
        {
            if (type == "audio")
            {
                audio_support_.insert(Util::Str2Num<int>(item));
                std::cout << LMSG << "audio support:" << item << std::endl;
            }
            else if (type == "video")
            {
                video_support_.insert(Util::Str2Num<int>(item));
                std::cout << LMSG << "video support:" << item << std::endl;
            }
        }

        cur_media_desc_ = type;
    }

    return 0;
}

int SdpParse::OnConnectionData(const std::string& line)
{
    return 0;
}
