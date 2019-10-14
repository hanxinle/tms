#ifndef __ADMIN_PROTOCOL_H__
#define __ADMIN_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <string>

class Epoller;
class Fd;
class IoBuffer;
class TcpSocket;

class AdminProtocol
{
public:
    AdminProtocol(Epoller* epoller, Fd* socket);
    ~AdminProtocol();

	int Parse(IoBuffer& io_buffer);
    int OnStop();
    int OnConnected();

    int EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count);

    TcpSocket* GetTcpSocket()
    {
        return (TcpSocket*)socket_;
    }

private: 
    int ProcAdminMsg(const std::string& admin_msg);

private:
	Epoller* epoller_;
    Fd* socket_;
};

#endif // __ADMIN_PROTOCOL_H__
