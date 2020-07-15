#ifndef __SDP_H__
#define __SDP_H__

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

class SdpSessionInfo {
 public:
  SdpSessionInfo();
  ~SdpSessionInfo();

  int parse_attribute(const std::string& attribute, const std::string& value);
  int encode(std::ostringstream& os);

 private:
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string ice_options_;
  std::string fingerprint_algo_;
  std::string fingerprint_;
  std::string setup_;
};

class SdpMediaPayload {
 public:
  SdpMediaPayload(int payload_type);
  ~SdpMediaPayload();

  int encode(std::ostringstream& os);

 public:
  int payload_type_;

  std::string encoding_name_;
  int clock_rate_;
  std::string encoding_param_;

  std::set<std::string> rtcp_fb_;
  std::string format_specific_param_;
};

class SdpMediaDesc {
  friend class Sdp;

 public:
  SdpMediaDesc(const std::string& type);
  ~SdpMediaDesc();

 public:
  int parse_line(const std::string& line);
  int encode(std::ostringstream& os);
  SdpMediaPayload* find_media_payload(int payload_type);

 private:
  int parse_attribute(const std::string& content);
  int parse_attr_rtpmap(const std::string& value);
  int parse_attr_rtcp_fb(const std::string& value);
  int parse_attr_fmtp(const std::string& value);
  int parse_attr_mid(const std::string& value);

 private:
  SdpSessionInfo session_info_;
  std::string type_;
  std::string mid_;
  std::set<std::string> protos;
  std::map<int, SdpMediaPayload*> payloads;
};

class Sdp {
 public:
  Sdp();
  ~Sdp();

  int parse(const std::string& sdp_str);
  int encode(std::ostringstream& os);

  static std::string get_error() { return error_desc_; }
  static void set_error(const std::string& err) { error_desc_ = err; }

 private:
  int parse_line(const std::string& line);

 private:
  int parse_origin(const std::string& content);
  int parse_version(const std::string& content);
  int parse_session_name(const std::string& content);
  int parse_timing(const std::string& content);
  int parse_attribute(const std::string& content);
  int parse_media_description(const std::string& content);
  int parse_attr_group(const std::string& content);

 private:
  SdpMediaDesc* cur_media_desc_;
  std::vector<SdpMediaDesc*> media_descs_;

 private:
  static std::string error_desc_;

 private:
  SdpSessionInfo session_info_;

  // version
  std::string version_;

  // origin
  std::string username_;
  std::string session_id_;
  std::string session_version_;
  std::string nettype_;
  std::string addrtype_;
  std::string unicast_address_;

  // session_name
  std::string session_name_;

  // timing
  int64_t start_time_;
  int64_t end_time_;

  bool rtcp_mux_;

  bool sendonly_;
  bool recvonly_;
  bool sendrecv_;
  bool inactive_;

  std::set<std::string> group_;
};

#endif  // __SDP_H__
