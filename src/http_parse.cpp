#include <iostream>
#include <map>

#include "common_define.h"
#include "http_parse.h"
#include "io_buffer.h"
#include "tcp_socket.h"

HttpParse::HttpParse()
    : r_pos_(-1)
    , n_pos_(-1)
    , m_pos_(-1)
    , x_pos_(-1)
    , next_pos_(0)
    , key_value_(false)
{
}

int HttpParse::Decode(IoBuffer& io_buffer)
{
	uint8_t* data = NULL;

    // 不要全部读完, 可能会把非http的也读到了
    int size = io_buffer.Peek(data, 0, io_buffer.Size());

    //std::cout << LMSG << "size:" << size << std::endl;

    int i = 0;
    int n = next_pos_;
    for ( ; i < size; ++n, ++i)
    {
        //std::cout << LMSG << "data[" << i << "]:" << (int)data[i] << "[" << data[i] << "]" << std::endl;
        if (data[i] == '\r')
        {
            r_pos_ = n;
        }
        else if (data[i] == '\n')
        {
            std::cout << LMSG << "n:" << n << ",i:" << i << ",r_pos_:" << r_pos_ << ",n_pos_:" << n_pos_ << std::endl;
            if (n == r_pos_ + 1)
            {
                if (n == n_pos_ + 2) // \r\n\r\n
                {
                    std::cout << LMSG << "http done" << std::endl;

                    int http_size = i + 1;
                    io_buffer.Skip(http_size);

                    return kSuccess;
                }
                else // \r\n
                {
                    key_value_ = false;

                    std::cout << LMSG << "http one line [" << key_ << "] => [" << value_ << "]" << std::endl;

                    // TODO: post, content type, content length
                    if (key_.find("GET") != std::string::npos)
                    {
                        //GET /test.flv HTTP/1.1


                        std::vector<std::string> vec = Util::SepStr(key_, " ");

                        if (vec.size() >= 2)
                        {
                            if (vec[0] != "GET" && vec[0] != "Get" && vec[0] != "get")
                            {
                                return kError;
                            }

                            std::string tmp_path;
                            std::string tmp_key;
                            std::string tmp_value;

                            int state = 0; // 0 = path, 1 = args_key 2 = args_value

                            for (size_t index = 0; index != vec[1].size(); ++index)
                            {
                                const char& ch = vec[1][index];

                                if (ch == '/')
                                {
                                    if (index != 0)
                                    {
                                        path_.push_back(tmp_path);
                                        tmp_path.clear();
                                    }
                                }
                                else if (ch == '?')
                                {
                                    if (state == 0)
                                    {
                                        state = 1;
                                    }
                                }
                                else if (ch == '&')
                                {
                                    args_[tmp_key] = tmp_value;
                                    tmp_key.clear();
                                    tmp_value.clear();
                                    state = 1;
                                }
                                else if (ch == '=')
                                {
                                    if (state == 1)
                                    {
                                        state = 2;
                                    }
                                    else if (state == 2)
                                    {
                                        tmp_value += ch;
                                    }
                                }
                                else
                                {
                                    if (state == 0)
                                    {
                                        tmp_path += ch;
                                    }
                                    else if (state == 1)
                                    {
                                        tmp_key += ch;
                                    }
                                    else if (state == 2)
                                    {
                                        tmp_value += ch;
                                    }
                                }
                            }

                            if (state == 0)
                            {
                                if (! tmp_path.empty())
                                {
                                    path_.push_back(tmp_path);
                                }
                            }
                            else
                            {
                                args_[tmp_key] = tmp_value;
                            }

                            // path debug
                            for (const auto& path : path_)
                            {
                                std::cout << LMSG << "path:" << path << std::endl;
                            }

                            if (path_.size() > 0)
                            {
                                file_name_ = path_[path_.size()-1];

                                auto dot_pos = file_name_.find(".");

                                if (dot_pos != std::string::npos)
                                {
                                    file_type_ = file_name_.substr(dot_pos + 1);
                                    file_name_ = file_name_.substr(0, dot_pos);
                                }

                                std::cout << LMSG << "file_name_:" << file_name_ << ",file_type_:" << file_type_ << std::endl;
                            }

                            // args debug
                            for (const auto& kv : args_)
                            {
                                std::cout << LMSG << "[" << kv.first << "]=>[" << kv.second << "]" << std::endl;
                            }
                        }

                        int s_count = 0;
                        int x_count = 0; // /
                        int d_count = 0; // .
                        for (const auto& ch : key_)
                        {
                            if (ch == ' ')
                            {
                                ++s_count;
                            }
                            else if (ch == '/')
                            {
                                ++x_count;
                            }
                            else if (ch == '.')
                            {
                                ++d_count;
                            }
                            else
                            {
                            }
                        }
                    }
                    else
                    {
                        header_kv_[key_] = value_;
                    }

                    key_.clear();
                    value_.clear();
                }
            }

            n_pos_ = n;
        }
        else if (data[i] == ':')
        {
            m_pos_ = n;

            if (key_value_)
            {
                value_ += data[i];
            }
            key_value_ = true;
        }
        else if (data[i] == ' ')
        {
            if (m_pos_ + 1 == n)
            {
            }
            else
            {
                if (key_value_)
                {
                    value_ += (char)data[i];
                }
                else
                {
                    key_ += (char)data[i];
                }
            }
        }
        else
        {
            if (key_value_)
            {
                value_ += (char)data[i];
            }
            else
            {
                key_ += (char)data[i];
            }
        }
    }

    next_pos_ = n;
    io_buffer.Skip(i);

    //std::cout << LMSG << "next_pos_:" <<next_pos_ << std::endl;

    return kNoEnoughData;
}

bool HttpParse::IsFlvRequest(std::string& app, std::string& stream)
{
    if (file_type_ == "flv" && path_.size() == 2)
    {
        app = path_[0];
        stream = file_name_;

        return true;
    }

    return false;
}

bool HttpParse::IsHlsRequest(std::string& app, std::string& stream)
{
    if (file_type_ == "hls" && path_.size() == 2)
    {
        app = path_[0];
        stream = file_name_;

        return true;
    }

    return false;
}
