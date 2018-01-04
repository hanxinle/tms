#ifndef __MEDIA_NODE_MGR_H__
#define __MEDIA_NODE_MGR_H__

#include <map>
#include <string>

#include "media_node_protocol.h"
#include "socket_handle.h"
#include "timer_handle.h"

using std::map;
using std::string;

class Epoller;
class Fd;
class MediaNodeProtocol;

class MediaNodeMgr : public SocketHandle, public TimerSecondHandle
{
public:
    MediaNodeMgr(Epoller* epoller);
    ~MediaNodeMgr();

    MediaNodeProtocol* GetOrCreateProtocol(Fd& socket);

    virtual int HandleRead(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleClose(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleError(IoBuffer& io_buffer, Fd& socket);
    virtual int HandleConnected(Fd& socket);

    virtual int HandleTimerInSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    bool InsertNode(const NodeInfo& node_info);
    set<NodeInfo> GetNode(const uint32_t& type, const uint16_t& num);

private:
    Epoller* epoller_;
    map<int, MediaNodeProtocol*> fd_protocol_;

    map<uint32_t, set<NodeInfo> > type_nodes_;
};

#endif // __MEDIA_NODE_MGR_H__
