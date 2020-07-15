#ifndef __WEBRTC_SESSION_MGR_H__
#define __WEBRTC_SESSION_MGR_H__

#include <string>
#include <unordered_map>

struct SessionInfo {
  std::string remote_ufrag;
  std::string remote_pwd;
  std::string local_ufrag;
  std::string local_pwd;
  std::string app;
  std::string stream;
};

class WebrtcSessionMgr {
 public:
  WebrtcSessionMgr();
  ~WebrtcSessionMgr();

  void AddSession(const std::string& ufrag, const SessionInfo& session_info);
  bool GetSession(const std::string& ufrag, SessionInfo& session_info);
  void DelSession(const std::string& ufrag);

 private:
  std::unordered_map<std::string, SessionInfo> session_infos;
};

extern WebrtcSessionMgr g_webrtc_session_mgr;

#endif  // __WEBRTC_SESSION_MGR_H__
