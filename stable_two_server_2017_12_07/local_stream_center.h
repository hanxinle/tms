#ifndef __LOCAL_STREAM_CENTER_H__
#define __LOCAL_STREAM_CENTER_H__

#include <string>

using std::string;

class MediaPublisher;

class LocalStreamCenter
{
public:
    LocalStreamCenter();
    ~LocalStreamCenter();

	bool RegisterStream(const string& app, const string& stream_name, MediaPublisher* media_publisher);
    bool UnRegisterStream(const string& app, const string& stream_name, MediaPublisher* media_publisher);
    MediaPublisher* GetMediaPublisherByAppStream(const string& app, const string& stream_name);
    bool IsAppStreamExist(const string& app, const string& stream_name);

private:
	map<string, map<string, MediaPublisher*>> app_stream_publisher_;
};

#endif // __LOCAL_STREAM_CENTER_H__
