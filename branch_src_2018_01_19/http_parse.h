#ifndef __HTTP_PARSE_H__
#define __HTTP_PARSE_H__

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

class IoBuffer;

using std::map;
using std::string;
using std::vector;

class HttpParse
{
public:
    HttpParse();
    ~HttpParse()
    {
    }

    int Decode(IoBuffer& io_buffer);

    bool IsFlvRequest(string& app, string& stream);
    bool IsHlsRequest(string& app, string& stream);

    string GetFileName()
    {
        return file_name_;
    }

    string GetFileType()
    {
        return file_type_;
    }

private:
    map<string, string> header_kv_;
    string host_;
    string url_;
    map<string, string> args_;

    int r_pos_;
    int n_pos_;
    int m_pos_;
    int x_pos_;
    int next_pos_;
    bool key_value_;

    string key_;
    string value_;
    string file_name_;
    string file_type_;

    vector<string> path_;
};

#endif // __HTTP_PARSE_H__
