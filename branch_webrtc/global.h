#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "local_stream_center.h"
#include "protocol.h"
#include "epoller.h"
#include "http_flv_mgr.h"
#include "http_hls_mgr.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"
#include "webrtc_mgr.h"
#include <openssl/ssl.h>

extern LocalStreamCenter 	    g_local_stream_center;
extern NodeInfo          	    g_node_info;
extern Epoller*        	        g_epoll;
extern HttpFlvMgr*     	        g_http_flv_mgr;
extern HttpHlsMgr*     	        g_http_hls_mgr;
extern MediaCenterMgr* 	        g_media_center_mgr;
extern MediaNodeDiscoveryMgr*   g_media_node_discovery_mgr;
extern RtmpMgr*        	        g_rtmp_mgr;
extern ServerMgr*      	        g_server_mgr;
extern WebrtcMgr*               g_webrtc_mgr;
extern SSL_CTX*                 g_tls_ctx;
extern SSL_CTX*                 g_dtls_ctx;

extern string                   g_dtls_fingerprint;
extern string                   g_local_ice_pwd;
extern string                   g_local_ice_ufrag;
extern string                   g_remote_ice_pwd;
extern string                   g_remote_ice_ufrag;

extern string                   g_server_ip;

#endif // __GLOBAL_H__
