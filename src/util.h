#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>

using std::string;

class Util
{
public:
	static string Bin2Hex(const uint8_t* buf, const size_t& len, const size_t& char_per_line = 32);
    static string Bin2Hex(const string& str);

	static uint64_t GetNowMs();
    static uint64_t GetNow();
    static uint64_t GetNowUs();
};

#endif // __UTIL_H__
