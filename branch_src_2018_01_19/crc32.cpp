#include "crc32.h"

#ifdef CRC32_MYSELF
uint32_t CRC32::GetCrc32(const uint8_t* data, int len)
{
	uint32_t i_crc = 0xffffffff;
    for(int i = 0; i < len; i++ )
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
#else
uint32_t CRC32::GetCrc32(const uint8_t* data, int len)
{
    uint32_t start = 0;
	uint32_t c = start ^ 0xFFFFFFFF;
	for (int i = 0; i < len; ++i) 
    {
	    c = crc32_table[(c ^ data[i]) & 0xFF] ^ (c >> 8); 
	}

	return c ^ 0xFFFFFFFF;
}
#endif
