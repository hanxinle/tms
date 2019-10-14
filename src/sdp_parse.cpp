#include <iostream>

#include "common_define.h"
#include "sdp_parse.h"
#include "util.h"

using namespace std;

SdpParse::SdpParse()
{
}

SdpParse::~SdpParse()
{
}

int SdpParse::Parse(const string& sdp)
{
    vector<string> v = Util::SepStr(sdp, CRLF);

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

int SdpParse::OnAttribute(const string& line)
{
    auto pos = line.find(":");

    if (pos == string::npos)
    {
        pos = -1;
    }

    string attribute = line.substr(0, pos);

    if (attribute == "ice-ufrag")
    {
        ice_ufrag_ = line.substr(pos + 1);
        cout << LMSG << "ice_ufrag_:" << ice_ufrag_ << endl;
    }
    else if (attribute == "ice-pwd")
    {
        ice_pwd_ = line.substr(pos + 1);
        cout << LMSG << "ice_pwd_:" << ice_pwd_ << endl;
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
        vector<string> v = Util::SepStr(line.substr(pos + 1), " ");
        if (cur_media_desc_ == "audio" && v.size() > 1)
        {
            audio_ssrc_ = Util::Str2Num<uint32_t>(v[0]);
            cout << LMSG << "audio_ssrc_:" << audio_ssrc_ << endl;
        }
        else if (cur_media_desc_ == "video" && v.size() > 1)
        {
            video_ssrc_ = Util::Str2Num<uint32_t>(v[0]);
            cout << LMSG << "video_ssrc_:" << video_ssrc_ << endl;
        }
    }
    else if (attribute == "rtpmap")
    {
        vector<string> v = Util::SepStr(line.substr(pos + 1), " ");

        if (v.size() == 2)
        {
            vector<string> codec_time = Util::SepStr(v[1], "/");
            if (codec_time.size() == 2)
            {
                cout << LMSG << "codec:" << codec_time[0] << ", clock:" << codec_time[1] << endl;
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

int SdpParse::OnMediaDesc(const string& line)
{
    vector<string> v = Util::SepStr(line, " ");

    if (v.size() > 3)
    {
        string type = v[0];
        string trans_protocol = v[2];

        v.erase(v.begin());
        v.erase(v.begin());
        v.erase(v.begin());

        for (const auto& item : v)
        {
            if (type == "audio")
            {
                audio_support_.insert(Util::Str2Num<int>(item));
                cout << LMSG << "audio support:" << item << endl;
            }
            else if (type == "video")
            {
                video_support_.insert(Util::Str2Num<int>(item));
                cout << LMSG << "video support:" << item << endl;
            }
        }

        cur_media_desc_ = type;
    }

    return 0;
}

int SdpParse::OnConnectionData(const string& line)
{
    return 0;
}
