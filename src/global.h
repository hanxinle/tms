#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <openssl/ssl.h>
#include "epoller.h"
#include "local_stream_center.h"

extern LocalStreamCenter g_local_stream_center;
extern Epoller* g_epoll;
extern SSL_CTX* g_tls_ctx;
extern SSL_CTX* g_dtls_ctx;
extern std::string g_dtls_fingerprint;
extern std::string g_local_ice_pwd;
extern std::string g_local_ice_ufrag;
extern std::string g_remote_ice_pwd;
extern std::string g_remote_ice_ufrag;
extern std::string g_server_ip;

#endif  // __GLOBAL_H__
