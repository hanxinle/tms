#ifndef __HTTP_PARSE_H__
#define __HTTP_PARSE_H__

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

class IoBuffer;

class HttpParse
{
public:
    HttpParse();
    ~HttpParse()
    {
    }

    int Decode(IoBuffer& io_buffer);

    bool IsFlvRequest(std::string& app, std::string& stream);
    bool IsHlsRequest(std::string& app, std::string& stream);

    std::string GetFileName()
    {
        return file_name_;
    }

    std::string GetFileType()
    {
        return file_type_;
    }

    bool GetHeaderKeyValue(const std::string& key, std::string& value)
    {
        auto iter = header_kv_.find(key);

        if (iter == header_kv_.end())
        {
            return false;
        }

        value = iter->second;

        return true;
    }

private:
    std::map<std::string, std::string> header_kv_;
    std::string host_;
    std::string url_;
    std::map<std::string, std::string> args_;

    int r_pos_;
    int n_pos_;
    int m_pos_;
    int x_pos_;
    int next_pos_;
    bool key_value_;

    std::string key_;
    std::string value_;
    std::string file_name_;
    std::string file_type_;

    std::vector<std::string> path_;
};

#endif // __HTTP_PARSE_H__
