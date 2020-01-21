#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>
#include <string.h>

#include <iostream>
#include <random>

#include "common_define.h"
#include "util.h"

const std::string letter = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const std::string num = "0123456789";

void Util::Daemon()
{
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;

	// Clear file creation mask.
	umask(0);

	// Get maximum number of file descriptors.
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
    {
        std::cout << LMSG << "getrlimit error:" << strerror(errno) << std::endl;
    }

	// Become a session leader to lose controlling TTY.
	if ((pid = fork()) < 0)
    {
        std::cout << LMSG << "fork error:" << strerror(errno) << std::endl;
        exit(-1);
    }
	else if (pid != 0) /* parent */
    {
	    exit(0);
    }

	setsid();

	// Ensure future opens won’t allocate controlling TTYs.
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
    {
        std::cout << LMSG << "sigaction ignore SIGHUP error:" << strerror(errno) << std::endl;
        exit(-1);
    }

	if ((pid = fork()) < 0)
    {
        std::cout << LMSG << "fork error:" << strerror(errno) << std::endl;
        exit(-1);
    }
	else if (pid != 0) // parent
    {
	    exit(0);
    }

	// Change the current working directory to the root so we won’t prevent file systems from being unmounted.
	if (chdir("/") < 0)
    {
        std::cout << LMSG << "chdir error:" << strerror(errno) << std::endl;
        exit(-1);
    }

	// Close all open file descriptors.
	if (rl.rlim_max == RLIM_INFINITY)
    {
	    rl.rlim_max = 1024;
    }

	for (int i = 0; i < (int)rl.rlim_max; i++)
    {
	    close(i);
    }

	// Attach file descriptors 0, 1, and 2 to /dev/null.
	int fd0 = open("/dev/null", O_RDWR);
	int fd1 = dup(0);
	int fd2 = dup(0);

    UNUSED(fd0);
    UNUSED(fd1);
    UNUSED(fd2);

    /* 我不需要syslog,保留下用法即可
	// Initialize the log file.
	openlog(cmd, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2) 
    {
	    syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
	    exit(1);
	}
    */
}

std::map<std::string, std::string> Util::ParseArgs(int argc, char* argv[])
{
    std::map<std::string, std::string> ret;
    int i = 1;
    while (i < argc && argv[i] != NULL)
    {   
        int index = 0;
        std::string opt;
        std::string val;
        while (argv[i][index] != '\0')
        {   
            if (argv[i][index] == '-')
            {   
            }   
            else
            {   
                opt += argv[i][index];
            }   
    
            ++index;
        }   
    
        ret[opt];
    
        if (argv[i+1] != NULL)
        {   
            bool b = false;
            index = 0;
            while (argv[i+1][index] != '\0')
            {   
                if (argv[i+1][index] == '-' && index < 2)
                {   
                    b = true;
                    break;
                }   
                else
                {   
                    val += argv[i+1][index];
                }

                ++index;
            }

            if (! b)
            {
                ++i;
                ret[opt] = val;
            }
        }

        ++i;
    }

    return ret;
}

std::string Util::Bin2Hex(const uint8_t* buf, const size_t& len, const size_t& char_per_line, const bool& printf_ascii, const std::string& prefix)
{   
    std::string hex; 
    std::string ascii = "    ";
        
    std::string line = prefix;
    for (size_t index = 0; index < len; ++index)
    {        
        char tmp[64] = {0}; 
        int bytes = sprintf(tmp, "%02X", buf[index]);
            
        if (printf_ascii)
        {
            if (isprint((char)buf[index]) && buf[index] != '\r' && buf[index] != '\n')
            {        
                ascii += (char)buf[index];
            }        
            else 
            {        
                ascii += "."; 
            }    
        }
            
        if (bytes > 0)  
        {        
            line.append(tmp, bytes);
        }    
            
        if (index % char_per_line == (char_per_line - 1))  
        {        
            if (printf_ascii)
            {
                line += ascii;
                ascii = "    ";
            }

            line.append("\n");

            hex += line;
            line = prefix;
        }        
        else 
        {        
            line.append(" ");
        }    
    }    

    hex += line;
        
    if (printf_ascii)
    {
        hex.append((char_per_line - (len % char_per_line)) * 3 - 1, ' ');
        hex += ascii;
    }
        
    return hex; 
}   

std::string Util::Bin2Hex(const std::string& str, const size_t& char_per_line, const bool& printf_ascii, const std::string& prefix)
{   
    return Bin2Hex((const uint8_t*)str.data(), str.length(), char_per_line, printf_ascii, prefix);
}

uint64_t Util::GetNowMs()
{   
    timeval tv; 
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}   

uint64_t Util::GetNow()
{   
    timeval tv; 
    gettimeofday(&tv, NULL);

    return tv.tv_sec;
}   

uint64_t Util::GetNowUs()
{   
    timeval tv; 
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000000UL + tv.tv_usec;
}

std::string Util::GetNowStr()
{
    char time_printf[256];

    timeval tv;
    gettimeofday(&tv, NULL);

    tm* time_struct = localtime(&tv.tv_sec);

    size_t ret = strftime(time_printf, sizeof(time_printf), "%Y-%m-%d %H:%M:%S", time_struct);

    if (ret > 0)
    {
        return std::string(time_printf, ret);
    }

    return "";
}

std::string Util::GetNowStrHttpFormat()
{
    char time_printf[256];

    timeval tv;
    gettimeofday(&tv, NULL);

    tm* time_struct = localtime(&tv.tv_sec);

    size_t ret = strftime(time_printf, sizeof(time_printf), "%a, %d %b %Y %T %z", time_struct);

    if (ret > 0)
    {
        return std::string(time_printf, ret);
    }

    return "";
}

std::string Util::GetNowMsStr()
{
    char time_printf[256];

    timeval tv;
    gettimeofday(&tv, NULL);

    tm* time_struct = localtime(&tv.tv_sec);

    size_t ret = strftime(time_printf, sizeof(time_printf), "%Y-%m-%d %H:%M:%S", time_struct);

    if (ret > 0)
    {
        time_printf[ret + 0] = '.';
        time_printf[ret + 1] = '0' + (tv.tv_usec/1000/100);
        time_printf[ret + 2] = '0' + (tv.tv_usec/1000/10%10);
        time_printf[ret + 3] = '0' + (tv.tv_usec/1000%10);
        time_printf[ret + 4] = '\0';

        return std::string(time_printf, ret + 4);
    }

    return "";
}

std::string Util::ReadFile(const std::string& file_name)
{
    std::string ret = "";

	int fd = open(file_name.c_str(), O_RDONLY, 0664);
    if (fd < 0)
    {   
        std::cout << LMSG << "open " << file_name << " faild, ret:" << fd << ",error:" << strerror(errno) << std::endl;
    }   
    else
    {
        while (true)
        {   
            char buf[4096];
            int bytes = read(fd, buf, sizeof(buf));

            if (bytes < 0)
            {   
            	std::cout << LMSG << "open " << file_name << " faild, ret:" << fd << ",error:" << strerror(errno) << std::endl;
                break;
            }   
            else if (bytes == 0)
            {   
                break;
            }   

            ret.append(buf, bytes);
        }
    }

	return ret;
}

std::vector<std::string> Util::SepStr(const std::string& input, const std::string& sep)
{
    std::vector<std::string> ret;

    size_t pre_pos = 0;
    while (true)
    {
        auto pos = input.find(sep, pre_pos);

        if (pos == std::string::npos)
        {
            ret.push_back(input.substr(pre_pos));
            break;
        }

        std::string tmp = input.substr(pre_pos, pos - pre_pos);

        if (tmp == sep)
        {
        }
        else
        {
            ret.push_back(tmp);
        }

        pre_pos = pos + sep.size();
    }

    return ret;
}

void Util::Replace(std::string& input, const std::string& from, const std::string& to) 
{
    size_t pos = 0;
    size_t next_pos = 0;

    while ((next_pos = input.find(from, pos)) != std::string::npos)
    {   
        input.replace(next_pos, from.length(), to);
    }   
}

std::string Util::GenRandom(const size_t& len)
{   
    std::string ret;
    std::random_device random_generate;

    for (size_t index = 0; index != len; ++index)
    {   
        ret += letter[random_generate() % letter.size()];
    }   

    return ret;
}

std::string Util::GenRandomNum(const size_t& len)
{   
    std::string ret;
    std::random_device random_generate;
    
    for (size_t index = 0; index != len; ++index)
    {   
        ret += num[random_generate() % num.size()];
    }

    return ret;
}
