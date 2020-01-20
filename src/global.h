#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "local_stream_center.h"
#include "epoller.h"
#include <openssl/ssl.h>

extern LocalStreamCenter 	            g_local_stream_center;
extern Epoller*        	                g_epoll;
extern SSL_CTX*                         g_tls_ctx;
extern SSL_CTX*                         g_dtls_ctx;
extern string                           g_dtls_fingerprint;
extern string                           g_local_ice_pwd;
extern string                           g_local_ice_ufrag;
extern string                           g_remote_ice_pwd;
extern string                           g_remote_ice_ufrag;
extern string                           g_server_ip;

#endif // __GLOBAL_H__
