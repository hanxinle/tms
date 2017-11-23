#include <sys/time.h>

#include "util.h"

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
