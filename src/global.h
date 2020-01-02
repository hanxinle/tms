#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "local_stream_center.h"
#include "epoller.h"
#include "srt_mgr.h"
#include "protocol_mgr.h"
#include "webrtc_mgr.h"
#include <openssl/ssl.h>

extern LocalStreamCenter 	            g_local_stream_center;
extern Epoller*        	                g_epoll;
extern ProtocolMgr<HttpFlvProtocol>*    g_http_flv_mgr;
extern ProtocolMgr<HttpHlsProtocol>*    g_http_hls_mgr;
extern ProtocolMgr<RtmpProtocol>*       g_rtmp_mgr;
extern ProtocolMgr<SrtProtocol>*        g_srt_mgr;
extern WebrtcMgr*                       g_webrtc_mgr;
extern SSL_CTX*                         g_tls_ctx;
extern SSL_CTX*                         g_dtls_ctx;
extern string                           g_dtls_fingerprint;
extern string                           g_local_ice_pwd;
extern string                           g_local_ice_ufrag;
extern string                           g_remote_ice_pwd;
extern string                           g_remote_ice_ufrag;
extern string                           g_server_ip;
extern int                              g_srt_client_fd;

#endif // __GLOBAL_H__
