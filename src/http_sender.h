#ifndef __HTTP_SENDER_H__
#define __HTTP_SENDER_H__

#include <string>
#include <utility>
#include <vector>

using std::pair;
using std::string;
using std::vector;

class HttpSender
{
public:
    HttpSender();
    ~HttpSender();

    int SetHeader(const string& key, const string& val);
    string Encode();
    int SetStatus(const string& status);
    void SetKeepAlive();
    void SetClose();
    void SetContent(const string& content)
    {
        content_ = content;
    }

    void SetContentType(const string& type);
    
private:
    string status_;
    vector<pair<string, string> > header_kv_;
    string content_;
};

#endif // __HTTP_SENDER_H__
