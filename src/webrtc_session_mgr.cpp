#include "webrtc_session_mgr.h"

WebrtcSessionMgr::WebrtcSessionMgr()
{
}

WebrtcSessionMgr::~WebrtcSessionMgr()
{
}

void WebrtcSessionMgr::AddSession(const std::string& ufrag, const SessionInfo& session_info)
{
    session_infos[ufrag] = session_info;
}

bool WebrtcSessionMgr::GetSession(const std::string& ufrag, SessionInfo& session_info)
{
    auto iter = session_infos.find(ufrag);

    if (iter == session_infos.end())
    {
        return false;
    }

    session_info = iter->second;
    return true;
}

void WebrtcSessionMgr::DelSession(const std::string& ufrag)
{
    session_infos.erase(ufrag);
}

WebrtcSessionMgr g_webrtc_session_mgr;
