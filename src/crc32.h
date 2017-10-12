#ifndef __CRC_32_H__
#define __CRC_32_H__

#include <iostream>

#include "common_define.h"

using std::cout;
using std::endl;

class CRC32
{
public:
    CRC32()
    {
	    for(uint32_t i = 0; i < 256; i++ )
        {   
            uint32_t k = 0;
            for(uint32_t j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1 ) 
            {
                k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0); 
            }

            crc32_table[i] = k;

            cout << LMSG << std::hex << k << std::dec << endl;
        }
    }

    ~CRC32()
    {
    }

    uint32_t GetCrc32(const uint8_t* data, int len);

private:
    uint32_t crc32_table[256];
};

#endif // __CRC_32_H__
