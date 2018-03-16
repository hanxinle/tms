#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "protocol.h"
#include "epoller.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "server_mgr.h"

extern NodeInfo                 g_node_info;
extern Epoller*        	        g_epoll;
extern MediaCenterMgr* 	        g_media_center_mgr;
extern MediaNodeDiscoveryMgr*   g_media_node_discovery_mgr;

extern string                   g_server_ip;

#endif // __GLOBAL_H__
