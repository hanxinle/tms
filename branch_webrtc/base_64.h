#ifndef __BASE_64_H__
#define __BASE_64_H__

#include <string>

using std::string;

class Base64
{
public:
    Base64();
    ~Base64();

    static int Encode(const uint8_t* input, const uint32_t& len, string& output);
    static int Encode(const string& input, string& output);
    static int Decode(const uint8_t* input, const uint32_t& len, string& output);
    static int Decode(const string& input, string& output);

private:

};

#endif // __BASE_64_H__
