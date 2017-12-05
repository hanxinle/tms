#ifndef __UTIL_H__
#define __UTIL_H__

#include <map>
#include <sstream>
#include <string>

using std::istringstream;
using std::ostringstream;
using std::map;
using std::string;

class Util
{
public:
	static map<string, string> ParseArgs(int argc, char* argv[]);
	static string Bin2Hex(const uint8_t* buf, const size_t& len, const size_t& char_per_line = 32);
    static string Bin2Hex(const string& str);

	static uint64_t GetNowMs();
    static uint64_t GetNow();
    static uint64_t GetNowUs();
    static string GetNowStr();
    static string GetNowMsStr();

	template<typename T>
    static string Num2Str(const T& t)
    {   
        ostringstream os; 
        os << t;

        return os.str();
    }   

    template<typename T>
    static T Str2Num(const string& str)
    {   
        T ret;
        istringstream is(str);

        is >> ret;

        return ret;
    }
};

#endif // __UTIL_H__
