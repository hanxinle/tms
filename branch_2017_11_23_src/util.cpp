#include <sys/time.h>

#include "util.h"

map<string, string> Util::ParseArgs(int argc, char* argv[])
{
    map<string, string> ret;
    int i = 1;
    while (i < argc && argv[i] != NULL)
    {   
        int index = 0;
        string opt;
        string val;
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

string Util::Bin2Hex(const uint8_t* buf, const size_t& len, const size_t& char_per_line)
{   
    string hex; 
    string ascii = "    ";
        
    for (size_t index = 0; index < len; ++index)
    {        
        char tmp[64] = {0}; 
        int bytes = sprintf(tmp, "%02X", buf[index]);
            
        if (isprint((char)buf[index]) && buf[index] != '\r' && buf[index] != '\n')
        {        
            ascii += (char)buf[index];
        }        
        else 
        {        
            ascii += "."; 
        }    
            
        if (bytes > 0)  
        {        
            hex.append(tmp, bytes);
        }    
            
        if (index % char_per_line == (char_per_line - 1))  
        {        
            hex += ascii;
            ascii = "    ";
            hex.append("\n");
        }        
        else 
        {        
            hex.append(" ");
        }    
    }    
        
    hex.append((char_per_line - (len % char_per_line)) * 3 - 1, ' ');
    hex += ascii;
        
    return hex; 
}   

string Util::Bin2Hex(const string& str)
{   
    return Bin2Hex((const uint8_t*)str.data(), str.length(), 32);
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

string Util::GetNowStr()
{
    char time_printf[256];

    timeval tv;
    gettimeofday(&tv, NULL);

    tm* time_struct = localtime(&tv.tv_sec);

    size_t ret = strftime(time_printf, sizeof(time_printf), "%Y-%m-%d %H:%M:%S", time_struct);

    if (ret > 0)
    {
        return string(time_printf, ret);
    }

    return "";
}

string Util::GetNowMsStr()
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

        return string(time_printf, ret + 4);
    }

    return "";
}
