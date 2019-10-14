#ifndef __LOCAL_STREAM_CENTER_H__
#define __LOCAL_STREAM_CENTER_H__

#include <set>
#include <string>
#include <map>

using std::set;
using std::string;
using std::map;

class MediaCenterMgr;
class MediaPublisher;
class MediaSubscriber;

class LocalStreamCenter
{
public:
    LocalStreamCenter();
    ~LocalStreamCenter();

    MediaPublisher* GetMediaPublisherByAppStream(const string& app, const string& stream);
    bool IsAppStreamExist(const string& app, const string& stream);

    bool RegisterStream(const string& app, const string& stream, MediaPublisher* media_publisher);
    bool UnRegisterStream(const string& app, const string& stream, MediaPublisher* media_publisher);

    MediaPublisher* _DebugGetRandomMediaPublisher(string& app, string& stream);

private:
	map<string, map<string, MediaPublisher*>> app_stream_publisher_;
};

#endif // __LOCAL_STREAM_CENTER_H__
