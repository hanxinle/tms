#ifndef __SOCKET_UTIL__
#define __SOCKET_UTIL__

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#include <string.h>

#include <iostream>
#include <string>

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
            std::cout << "inet_pton err:" << strerror(errno) << ",ip:" << ip_str << std::endl;
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
            std::cout << "inet_ntop err:" << strerror(errno) << std::endl;
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

    inline int CreateTcpSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0)
        {
            std::cout << "socket err:" << strerror(errno) << std::endl;
        }

        return fd;
    }

    inline int CreateNonBlockTcpSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

        if (fd < 0)
        {
            std::cout << "socket err:" << strerror(errno) << std::endl;
        }

        return fd;
    }

    inline int ReuseAddr(const int& fd)
    {
        int i = 1;
        int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
        if (ret < 0)
        {
            std::cout << "setsockopt err:" << strerror(errno) << std::endl;
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
            std::cout << "getaddrinfo err:" << strerror(errno) << std::endl;
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
            std::cout << "connect err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int ConnectHost(const int& fd, const std::string& host, const uint16_t& port)
    {
        std::string ip = GetIpByHost(host);

        if (ip.empty())
        {
            return -1;
        }

        std::cout << "host:" << host << ",ip:" << ip << std::endl;

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
            std::cout << "bind err:" << strerror(errno) << std::endl;
        }

        return ret;
    }

    inline int Listen(const int& fd)
    {
        int ret = listen(fd, 64);

        if (ret < 0)
        {
            std::cout << "listen err:" << strerror(errno) << std::endl;
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
            std::cout << "accept err:" << strerror(errno) << std::endl;
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
    	    std::cout << "fcntl err:" << strerror(errno) << std::endl;
    	    return flags;
    	}

    	flags |= O_NONBLOCK;
    	if (fcntl(fd, F_SETFL, flags) < 0)
    	{
    	    std::cout << "fcntl err:" << strerror(errno) << std::endl;
    	    return -1;
    	}

    	return 0;
    }

} // namespace socket_util

#endif // __SOCKET_UTIL__
