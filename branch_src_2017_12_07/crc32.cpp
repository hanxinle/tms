#include "crc32.h"

uint32_t CRC32::GetCrc32(const uint8_t* data, int len)
{
	uint32_t i_crc = 0xffffffff;
    for(int i = 0; (size_t)i < len; i++ )
	{
    	i_crc = (i_crc << 8) ^ crc32_table[((i_crc >> 24) ^ data[i]) & 0xff];
	}

    for (size_t i = 0; i != sizeof(crc32_table)/sizeof(uint32_t); ++i)
    {
        //cout << LMSG << "i:" << i << ":" << std::hex << crc32_table[i] << std::dec << endl;
    }

    //cout << LMSG << "i_crc:" << i_crc << endl;

    return i_crc;
}
