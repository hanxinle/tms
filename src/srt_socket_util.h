#ifndef __SRT_SOCKET_UTIL_H__
#define __SRT_SOCKET_UTIL_H__

#include <string>

#include "srt/srt.h"
#include "srt/udt.h"

#include "util.h"
#include "socket_util.h"
#include "srt_socket_util.h"

using namespace std;

namespace srt_socket_util
{

inline int CreateSrtSocket()
{
    int ret = srt_socket(AF_INET, SOCK_DGRAM, 0);

    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_socket, ret=" << ret << endl;
    }

    return ret;
}

inline int Bind(const int& fd, const string& ip, const uint16_t& port)
{
    sockaddr_in sa;
    socket_util::IpPortToSocketAddr(ip, port, sa);
    int ret = srt_bind(fd, (sockaddr*)&sa, sizeof(sa));

    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_bind, ret=" << ret << endl;
    }

    return ret;
}

inline int Connect(const int& fd, const std::string& ip, const uint16_t& port)
{
    sockaddr_in sa;
    socket_util::IpPortToSocketAddr(ip, port, sa);
    int ret = srt_connect(fd, (sockaddr*)&sa, sizeof(sa));

    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_connect, ret=" << ret << endl;
    }

    return ret;
}

inline int Listen(const int& fd)
{
    int ret = srt_listen(fd, 5);
    
    if (ret == SRT_ERROR)
    {
        cout << LMSG << "srt_listen, ret=" << ret << endl;
    }

    return ret;
}

inline int ReuseAddr(const int& fd)
{
	bool reuse = true;
    int ret = srt_setsockopt(fd, 0, SRTO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret == SRT_ERROR)
    {   
        cout << LMSG << "srt_setsockopt, ret=" << ret << endl;
    }

    return ret;
}

inline int SetBlock(const int& fd, const bool& block)
{
    int ret = srt_setsockopt(fd, 0, SRTO_RCVSYN, &block, sizeof(block));
    if (ret == SRT_ERROR)
    {   
        cout << LMSG << "srt_setsockopt, ret=" << ret << endl;
    }

    ret = srt_setsockopt(fd, 0, SRTO_SNDSYN, &block, sizeof(block));
    if (ret == SRT_ERROR)
    {   
        cout << LMSG << "srt_setsockopt, ret=" << ret << endl;
    }

    return ret;
}

} // namespace srt_socket_util

#endif // __SRT_SOCKET_UTIL_H__
