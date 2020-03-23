#include "sdp.h"

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <vector>

const std::string kCRLF = "\r\n";

std::string Sdp::error_desc_ = "";

#define FETCH(is,word) \
if (! (is >> word)) {\
    Sdp::set_error("fetch word");\
    return -1;\
}\

#define FETCH_WITH_DELIM(is,word,delim) \
if (! getline(is,word,delim)) {\
    return -1;\
}\

#define PRINT(v) \
std::cout << #v << "=" << v << std::endl

static std::vector<std::string> split_str(const std::string& str, const std::string& delim)
{
    std::vector<std::string> ret;
    size_t pre_pos = 0;
    std::string tmp;
    size_t pos = 0;
    do {
        pos = str.find(delim, pre_pos);
        tmp = str.substr(pre_pos, pos - pre_pos);
        ret.push_back(tmp);
        pre_pos = pos + delim.size();
    } while (pos != std::string::npos);

    return ret;
}

SdpSessionInfo::SdpSessionInfo()
{
}

SdpSessionInfo::~SdpSessionInfo()
{
}

int SdpSessionInfo::parse_attribute(const std::string& attribute, const std::string& value)
{
    if (attribute == "ice-ufrag") {
        ice_ufrag_ = value;
    } else if (attribute == "ice-pwd") {
        ice_pwd_ = value;
    } else if (attribute == "ice-options") {
        ice_options_ = value;
    } else if (attribute == "fingerprint") {
        std::istringstream is(value);
        FETCH(is, fingerprint_algo_);
        FETCH(is, fingerprint_);
    } else if (attribute == "setup") {
        setup_ = value;
    } 

    return 0;
}

int SdpSessionInfo::encode(std::ostringstream& os)
{
    if (! ice_ufrag_.empty()) {
        os << "a=ice-ufrag:" << ice_ufrag_ << kCRLF;
    }
    if (! ice_pwd_.empty()) {
        os << "a=ice-pwd:" << ice_pwd_ << kCRLF;
    }
    if (! ice_options_.empty()) {
        os << "a=ice-options:" << ice_options_ << kCRLF;
    }
    if (! fingerprint_algo_.empty() && ! fingerprint_.empty()) {
        os << "a=fingerprint:" << fingerprint_algo_ << " " << fingerprint_ << kCRLF;
    }
    if (! setup_.empty()) {
        os << "a=setup:" << setup_ << kCRLF;
    }

    return 0;
}

SdpMediaPayload::SdpMediaPayload(int payload_type)
{
    payload_type_ = payload_type;
}

SdpMediaPayload::~SdpMediaPayload()
{
}

int SdpMediaPayload::encode(std::ostringstream& os)
{
    os << "a=rtpmap:" << payload_type_ << " " << encoding_name_ << "/" << clock_rate_;
    if (! encoding_param_.empty()) {
        os << "/" << encoding_param_;
    }
    os << kCRLF;

    for (std::set<std::string>::iterator iter = rtcp_fb_.begin(); iter != rtcp_fb_.end(); ++iter) {
        os << "a=rtcp-fb:" << payload_type_ << " " << *iter << kCRLF;
    }

    if (! format_specific_param_.empty()) {
        os << "a=fmtp:" << payload_type_ << " " << format_specific_param_ << kCRLF;
    }

    return 0;
}

SdpMediaDesc::SdpMediaDesc(const std::string& type)
{
    type_ = type;
}

SdpMediaDesc::~SdpMediaDesc()
{
}

SdpMediaPayload* SdpMediaDesc::find_media_payload(int payload_type)
{
    std::map<int, SdpMediaPayload*>::iterator iter = payloads.find(payload_type);
    if (iter == payloads.end()) {
        return NULL;
    }

    return iter->second;
}

int SdpMediaDesc::parse_line(const std::string& line)
{
    std::string content = line.substr(2);

    switch (line[0]) {
        case 'a': {
            return parse_attribute(content);
        }
        case 'c': {
            break;
        }
        default: {
            break;
        }
    }

    return 0;
}

int SdpMediaDesc::encode(std::ostringstream& os)
{
    int ret = 0;

    os << "m=" << type_ << " ";
    for (std::set<std::string>::iterator iter = protos.begin(); iter != protos.end(); ++iter) {
        if (iter != protos.begin()) {
            os << "/";
        }
        os << *iter;
    }

    for (std::map<int, SdpMediaPayload*>::iterator iter = payloads.begin(); iter != payloads.end(); ++iter) {
        os << " " << iter->first;
    }

    os << kCRLF;

    if ((ret = session_info_.encode(os)) != 0) {
        return ret;
    }

    os << "a=mid:" << mid_ << kCRLF;

    for (std::map<int, SdpMediaPayload*>::iterator iter = payloads.begin(); iter != payloads.end(); ++iter) {
        if ((ret = iter->second->encode(os)) != 0) {
            return ret;
        }
    }

    return 0;
}

int SdpMediaDesc::parse_attribute(const std::string& content)
{
    std::string attribute = "";
    std::string value = "";
    size_t pos = content.find_first_of(":");

    if (pos != std::string::npos) {
        attribute = content.substr(0, pos);
        value = content.substr(pos + 1);
    }

    if (attribute == "extmap") {
        // TODO:We don't parse "extmap" currently.
        return 0;
    } else if (attribute == "rtpmap") {
        return parse_attr_rtpmap(value);
    } else if (attribute == "rtcp-fb") {
        return parse_attr_rtcp_fb(value);
    } else if (attribute == "fmtp") {
        return parse_attr_fmtp(value);
    } else if (attribute == "mid") {
        return parse_attr_mid(value);
	} else {
        return session_info_.parse_attribute(attribute, value);
    }

    return 0;
}

int SdpMediaDesc::parse_attr_rtpmap(const std::string& value)
{
    // @see: https://tools.ietf.org/html/rfc4566#page-25
    // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]

    std::istringstream is(value);

    int payload_type = 0;
    FETCH(is, payload_type);
    PRINT(payload_type);

    SdpMediaPayload* payload = find_media_payload(payload_type);
    if (payload == NULL) {
        Sdp::set_error("payload type no found");
        return -1;
    }

    std::string word;
    FETCH(is, word);

    std::vector<std::string> vec = split_str(word, "/");
    if (vec.size() < 2) {
        Sdp::set_error("invalid rtpmap");
        return -1;
    }

    payload->encoding_name_ = vec[0];
    payload->clock_rate_ = atoi(vec[1].c_str());

    if (vec.size() == 3) {
        payload->encoding_param_ = vec[2];
    }

    return 0;
}

int SdpMediaDesc::parse_attr_rtcp_fb(const std::string& value)
{
    // @see: https://tools.ietf.org/html/rfc5104#section-7.1

    std::istringstream is(value);

    int payload_type = 0;
    FETCH(is, payload_type);

    SdpMediaPayload* payload = find_media_payload(payload_type);
    if (payload == NULL) {
        Sdp::set_error("invalid rtcp_fb");
        return -1;
    }

    std::string rtcp_fb = is.str().substr(is.tellg());
    while (! rtcp_fb.empty() && rtcp_fb[0] == ' ') {
        rtcp_fb.erase(0, 1);
    }

    payload->rtcp_fb_.insert(rtcp_fb);

    return 0;
}

int SdpMediaDesc::parse_attr_fmtp(const std::string& value)
{
    // @see: https://tools.ietf.org/html/rfc4566#page-30
    // a=fmtp:<format> <format specific parameters>

    std::istringstream is(value);

    int payload_type = 0;
    FETCH(is, payload_type);

    SdpMediaPayload* payload = find_media_payload(payload_type);
    if (payload == NULL) {
        Sdp::set_error("invalid fmtp");
        return -1;
    }

    std::string word;
    FETCH(is, word);

    payload->format_specific_param_ = word;

    return 0;
}

int SdpMediaDesc::parse_attr_mid(const std::string& value)
{
    std::istringstream is(value);

    FETCH(is, mid_);

    return 0;
}

Sdp::Sdp()
{
    cur_media_desc_ = NULL;

    rtcp_mux_ = false;

    sendrecv_ = false;
    recvonly_ = false;
    sendonly_ = false;
    inactive_ = false;
}

Sdp::~Sdp()
{
}

int Sdp::parse(const std::string& sdp_str)
{
    // All webrtc sdp annotated example
    // @see: https://tools.ietf.org/html/draft-ietf-rtcweb-sdp-11
    std::istringstream is(sdp_str);
    std::string line;
    while (getline(is, line)) {
        if (line.size() < 2 || line[1] != '=') {
            error_desc_ = "invalid sdp";
            return -1;
        }
        if (! line.empty() && line[line.size()-1] == '\r') {
            line.erase(line.size()-1, 1);
        }

        if (parse_line(line) != 0) {
            return -1;
        }
    }

    if (cur_media_desc_ != NULL) {
        media_descs_.push_back(cur_media_desc_);
        cur_media_desc_ = NULL;
    }

    return 0;
}

int Sdp::encode(std::ostringstream& os)
{
    int ret = 0;

    os << "v=" << version_ << kCRLF;
    os << "o=" << username_ << " " << session_id_ << " " << session_version_ << " " << nettype_ << " " << addrtype_ << " " << unicast_address_ << kCRLF;
    os << "s=" << session_name_ << kCRLF;
    os << "t=" << start_time_ << " " << end_time_ << kCRLF;

    if (! group_.empty()) {
        os << "a=group:BUNDLE";
        for (std::set<std::string>::iterator iter = group_.begin(); iter != group_.end(); ++iter) {
            os << " " << *iter;
        }
        os << kCRLF;
    }

    os << "a=msid-semantic: WMS" << kCRLF;

    if ((ret = session_info_.encode(os)) != 0) {
        return ret;
    }

    for (std::vector<SdpMediaDesc*>::iterator iter = media_descs_.begin(); iter != media_descs_.end(); ++iter) {
        if ((ret = (*iter)->encode(os)) != 0) {
            return ret;
        }
    }

    return ret;
}

int Sdp::parse_line(const std::string& line)
{
    std::string content = line.substr(2);

    switch (line[0]) {
        case 'o': {
            return parse_origin(content);
        }
        case 'v': {
            return parse_version(content);
        }
        case 's': {
            return parse_session_name(content);
        }
        case 't': {
            return parse_timing(content);
        }
        case 'a': {
            if (cur_media_desc_ != NULL) {
                return cur_media_desc_->parse_line(line);
            }
            return parse_attribute(content);
        }
        case 'm': {
            return parse_media_description(content);
        }
        case 'c': {
            break;
        }
        default: {
            break;
        }
    }

    return 0;
}

int Sdp::parse_origin(const std::string& content)
{
    std::cout << "content=" << content << std::endl;
    // @see: https://tools.ietf.org/html/rfc4566#section-5.2
    // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
    // eg. o=- 9164462281920464688 2 IN IP4 127.0.0.1
    std::istringstream is(content);

    FETCH(is, username_);
    FETCH(is, session_id_);
    FETCH(is, session_version_);
    FETCH(is, nettype_);
    FETCH(is, addrtype_);
    FETCH(is, unicast_address_);

    return 0;
}

int Sdp::parse_version(const std::string& content)
{
    // @see: https://tools.ietf.org/html/rfc4566#section-5.1

    std::istringstream is(content);

    FETCH(is, version_);
    PRINT(version_);

    return 0;
}

int Sdp::parse_session_name(const std::string& content)
{
    // @see: https://tools.ietf.org/html/rfc4566#section-5.3
    // s=<session name>

    std::istringstream is(content);

    FETCH(is, session_name_);
    PRINT(session_name_);

    return 0;
}

int Sdp::parse_timing(const std::string& content)
{
    // @see: https://tools.ietf.org/html/rfc4566#section-5.9
    // t=<start-time> <stop-time>
    
    std::istringstream is(content);

    FETCH(is, start_time_);
    PRINT(start_time_);

    FETCH(is, end_time_);
    PRINT(end_time_);

    return 0;
}

int Sdp::parse_attribute(const std::string& content)
{
    // @see: https://tools.ietf.org/html/rfc4566#section-5.13
    // a=<attribute>
    // a=<attribute>:<value>

    std::string attribute = "";
    std::string value = "";
    size_t pos = content.find_first_of(":");

    if (pos != std::string::npos) {
        attribute = content.substr(0, pos);
        value = content.substr(pos + 1);
    }

    if (attribute == "group") {
        return parse_attr_group(value);
    } else if (attribute == "msid-semantic") {
        if (value.find("WMS") == std::string::npos) {
            Sdp::set_error("support WMS only");
            return -1;
        }
    } else if (attribute == "rtcp-mux") {
        rtcp_mux_ = true;
    } else if (attribute == "recvonly") {
        recvonly_ = true;
    } else if (attribute == "sendonly") {
        sendonly_ = true;
    } else if (attribute == "sendrecv") {
        sendrecv_ = true;
    } else if (attribute == "inactive") {
        inactive_ = true;
    } else {
        return session_info_.parse_attribute(attribute, value);
    }

    return 0;
}

int Sdp::parse_attr_group(const std::string& value)
{
    std::istringstream is(value);

    std::string group_policy;
    FETCH(is, group_policy);

    if (group_policy != "BUNDLE") {
        Sdp::set_error("support BUNDLE only");
        return -1;
    }

    std::string bundle;
    while (is >> bundle) {
        group_.insert(bundle);
    }

    return 0;
}

int Sdp::parse_media_description(const std::string& content)
{
    // @see: https://tools.ietf.org/html/rfc4566#section-5.14
    // m=<media> <port> <proto> <fmt> ...
    // m=<media> <port>/<number of ports> <proto> <fmt> ...
    if (cur_media_desc_ != NULL) {
        media_descs_.push_back(cur_media_desc_);
        cur_media_desc_ = NULL;
    }

    std::istringstream is(content);

    std::string media;
    FETCH(is, media);

    std::string port;
    FETCH(is, port);

    std::string proto;
    FETCH(is, proto);

    cur_media_desc_ = new SdpMediaDesc(media);
    std::vector<std::string> vec = split_str(proto, "/");
    cur_media_desc_->protos = std::set<std::string>(vec.begin(), vec.end());

    int fmt;
    while (is >> fmt) {
        cur_media_desc_->payloads.insert(std::make_pair(fmt, new SdpMediaPayload(fmt)));
    }

    return 0;
}
