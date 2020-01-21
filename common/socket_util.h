#ifndef __SOCKET_UTIL__
#define __SOCKET_UTIL__

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#include <string.h>

#include <iostream>
#include <string>

#include "common_define.h"

namespace socket_util
{
    inline int IpStr2Num(const std::string& ip_str, uint32_t& ip_num)
    {
        in_addr addr;

        int ret = inet_pton(AF_INET, ip_str.c_str(), &addr);

        if (ret == 1)
        {
            ip_num = addr.s_addr;
        }
        else
        {
            std::cout << LMSG << "inet_pton err:" << strerror(errno) << ",ip:" << ip_str << std::endl;
            ret = -1;
        }

        return ret;
    }

    inline std::string IpNum2Str(const uint32_t& ip_num)
    {
        in_addr addr = {0};
        char buf[INET6_ADDRSTRLEN] = {0};
        std::string str;

        addr.s_addr = ip_num;

        const char* p = inet_ntop(AF_INET, &addr, buf, sizeof(buf));

        if (p != NULL)
        {
            str.assign(p);
        }
        else
        {
            std::cout << LMSG << "inet_ntop err:" << strerror(errno) << std::endl;
        }
        
        return str;
    }

    inline int CreateSocketAddrInet(const std::string& ip, const uint16_t& port, sockaddr_in& addr_in)
    {
        memset(&addr_in, 0, sizeof(addr_in));

        uint32_t ip_num = 0;
        if (IpStr2Num(ip, ip_num) < 0)
        {
            return -1;
        }

        addr_in.sin_family = AF_INET;
        addr_in.sin_addr.s_addr = ip_num;
        addr_in.sin_port = htons(port);

        return 0;
    }

    inline void SocketAddrInetToIpPort(const sockaddr_in& addr_in, std::string& ip, uint16_t& port)
    {
        ip = IpNum2Str(addr_in.sin_addr.s_addr);
        port = ntohs(addr_in.sin_port);
    }

	inline int IpStrToNet(const std::string& str, uint32_t& net)
	{
	    in_addr out = {0};
	
	    int ret = inet_pton(AF_INET, str.c_str(), &out);
	
	    if (ret <= 0)
	    {
	        std::cout << LMSG << "inet_pton" << std::endl;
	        return -1;
	    }
	
	    net = out.s_addr;
	
	    return 0;
	}
	
	inline int IpNetToStr(const uint32_t& net, std::string& str)
	{
	    in_addr in = {0};
	    in.s_addr = net;
	
	    char out_buf[INET6_ADDRSTRLEN] = {0};
	
	    const char* pc = inet_ntop(AF_INET, &in, out_buf, sizeof(out_buf));
	
	    if (pc == NULL)
	    {
	        std::cout << LMSG << "inet_ntop" << std::endl;
	        return -1;
	    }
	
	    str.assign(pc);
	
	    return 0;
	}
	
	inline int IpStrToHost(const std::string& str, uint32_t& host)
	{
	    uint32_t net = 0;
	    int ret = IpStrToNet(str, net);
	
	    host = be32toh(net);
	
	    return ret;
	}


	inline int IpPortToSocketAddr(const std::string& ip, const uint16_t& port, sockaddr_in& addr)
	{
	    int ret = 0;
	
	    bzero(&addr, sizeof(addr));
	
	    uint32_t net = 0;
	    ret = IpStrToNet(ip, net);
	
	    if (ret < 0)
	    {
	        return ret;
	    }
	
	    addr.sin_family = AF_INET;
	    addr.sin_addr.s_addr = net;
	    addr.sin_port = htons(port);
	
	    return 0;
	}
	
	inline int SocketAddrToIpPort(const sockaddr_in& addr, std::string& ip, uint16_t& port)
	{
	    uint32_t net = addr.sin_addr.s_addr;
	    int ret = IpNetToStr(net, ip);
	
	    if (ret < 0)
	    {
	        return -1;
	    }
	
	    port = ntohs(addr.sin_port);
	
	    return 0;
	}
	
	inline int IpHostToStr(const uint32_t& host, std::string& str)
	{
	    return IpNetToStr(be32toh(host), str);
	}
	
    inline int CreateTcpSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0)
        {
            std::cout << LMSG << "socket err:" << strerror(errno) << std::endl;
        }

        return fd;
    }

    inline int CreateNonBlockTcpSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

        if (fd < 0)
        {
            std::cout << LMSG << "socket err:" << strerror(errno) << std::endl;
        }

        return fd;
    }

    inline int CreateUdpSocket()
    {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);

        if (fd < 0)
        {
            std::cout << LMSG << "socket err:" << strerror(errno) << std::endl;
        }

        return fd;
    }

    inline int CreateNonBlockUdpSocket()
    {
        int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

        if (fd < 0)
        {
            std::cout << LMSG << "socket err:" << strerror(errno) << std::endl;
        }

        return fd;
    }

    inline int ReuseAddr(const int& fd)
    {
        int i = 1;
        int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
        if (ret < 0)
        {
            std::cout << LMSG << "setsockopt err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int NoCloseWait(const int& fd)
    {
        linger st_linger;
        st_linger.l_onoff = 1;  //在close socket调用后, 但是还有数据没发送完毕的时候容许逗留
        st_linger.l_linger = 0; //容许逗留的时间为0秒
        int ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, &st_linger, sizeof(st_linger));
        if (ret < 0)
        {
            std::cout << LMSG << "setsockopt err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int SetSendBufSize(const int& fd, const int& send_buf_size, const bool& force = false)
    {
        int opt_name = SO_SNDBUF;
        if (force)
        {
            opt_name = SO_SNDBUFFORCE;
        }

        int ret = setsockopt(fd, SOL_SOCKET, opt_name, (void*)&send_buf_size, sizeof(send_buf_size));

        if (ret < 0)
        {
            std::cout << LMSG << "setsockopt err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int SetRecvBufSize(const int& fd, const int& send_buf_size, const bool& force = false)
    {
        int opt_name = SO_RCVBUF;
        if (force)
        {
            opt_name = SO_RCVBUFFORCE;
        }

        int ret = setsockopt(fd, SOL_SOCKET, opt_name, (void*)&send_buf_size, sizeof(send_buf_size));

        if (ret < 0)
        {
            std::cout << LMSG << "setsockopt err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int GetSendBufSize(const int& fd, int& send_buf_size)
    {
        send_buf_size = 0;
        socklen_t send_buf_size_len = sizeof(send_buf_size);
        int ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, &send_buf_size_len);

        if (ret < 0)
        {
            std::cout << LMSG << "getsockopt err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int GetRecvBufSize(const int& fd, int& send_buf_size)
    {
        send_buf_size = 0;
        socklen_t send_buf_size_len = sizeof(send_buf_size);
        int ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &send_buf_size, &send_buf_size_len);

        if (ret < 0)
        {
            std::cout << LMSG << "getsockopt err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline std::string GetIpByHost(const std::string& host)
    {
        std::string ret;

        addrinfo ai;
        memset(&ai, 0, sizeof(addrinfo));

        addrinfo* result = NULL;

        ai.ai_family = AF_INET;
        ai.ai_socktype = SOCK_DGRAM;

        int err = getaddrinfo(host.c_str(), NULL, &ai, &result);
        if (err != 0)
        {
            std::cout << LMSG << "getaddrinfo err:" << strerror(errno) << std::endl;
        }
        else
        {
           for (addrinfo* rp = result; rp != NULL; rp = rp->ai_next) 
           {
               sockaddr_in* p = (sockaddr_in*)rp->ai_addr;
               ret = IpNum2Str(p->sin_addr.s_addr);
               break;
           }

           freeaddrinfo(result);
        }

        return ret;
    }

    inline int Connect(const int& fd, const std::string& ip, const uint16_t& port)
    {
        sockaddr_in addr;
        int ret = CreateSocketAddrInet(ip, port, addr);

        if (ret < 0)
        {
            return -1;
        }

        ret = connect(fd, (sockaddr*)&addr, sizeof(addr));
        if (ret < 0)
        {
            std::cout << LMSG << "connect err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int ConnectHost(const int& fd, const std::string& host, const uint16_t& port)
    {
        std::string ip = GetIpByHost(host);

        if (ip.empty())
        {
            return Connect(fd, host, port);
        }

        std::cout << LMSG << "host:" << host << ",ip:" << ip << std::endl;

        return Connect(fd, ip, port);
    }

    inline int Bind(const int& fd, const std::string& ip, const uint16_t& port)
    {
        sockaddr_in addr;
        int ret = CreateSocketAddrInet(ip, port, addr);

        if (ret < 0)
        {
            return -1;
        }

        ret = bind(fd, (sockaddr*)&addr, sizeof(addr));
        if (ret < 0)
        {
            std::cout << LMSG << "bind err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int Listen(const int& fd)
    {
        int ret = listen(fd, 64);

        if (ret < 0)
        {
            std::cout << LMSG << "listen err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int Accept(const int& fd, std::string& ip, uint16_t& port)
    {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);

        int ret = accept(fd, (sockaddr*)&addr, &len);
        if (ret < 0)
        {
            std::cout << LMSG << "accept err:" << strerror(errno) << std::endl;
        }
        else
        {
            SocketAddrInetToIpPort(addr, ip, port);
        }

        return ret;
    }

    inline int SetNonBlock(const int& fd)
    {
		int flags = fcntl(fd, F_GETFL, 0);
    	if (flags < 0)
    	{
    	    std::cout << LMSG << "fcntl err:" << strerror(errno) << std::endl;
    	    return flags;
    	}

    	flags |= O_NONBLOCK;
    	if (fcntl(fd, F_SETFL, flags) < 0)
    	{
    	    std::cout << LMSG << "fcntl err:" << strerror(errno) << std::endl;
    	    return -1;
    	}

    	return 0;
    }

    inline int GetSocketError(const int& fd, int& err)
    {
        socklen_t err_len = sizeof(err);
        int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);

        if (ret != 0)
        {
            std::cout << LMSG << "getsockopt err:" << strerror(errno) << std::endl;
            return -1;
        }

        std::cout << LMSG << "fd:" << fd << ",err:" << err << std::endl;

        return 0;
    }

} // namespace socket_util

#endif // __SOCKET_UTIL__
