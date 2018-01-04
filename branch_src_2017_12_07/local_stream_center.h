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

	bool RegisterStream(const string& app, const string& stream_name, MediaPublisher* media_publisher);
    bool UnRegisterStream(const string& app, const string& stream_name, MediaPublisher* media_publisher);
    MediaPublisher* GetMediaPublisherByAppStream(const string& app, const string& stream_name);
    bool IsAppStreamExist(const string& app, const string& stream_name);

    bool AddAppStreamPendingSubscriber(const string& app, const string& stream, MediaSubscriber* media_subscriber);
    set<MediaSubscriber*> GetAppStreamPendingSubscriber(const string& app, const string& stream);

    MediaPublisher* _DebugGetRandomMediaPublisher(string& app, string& stream);

private:
	map<string, map<string, MediaPublisher*>> app_stream_publisher_;
	map<string, map<string, set<MediaSubscriber*>>> app_stream_pending_subscriber_;
};

#endif // __LOCAL_STREAM_CENTER_H__
