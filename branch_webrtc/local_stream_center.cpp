#include "common_define.h"
#include "global.h"
#include "local_stream_center.h"
#include "media_subscriber.h"

LocalStreamCenter::LocalStreamCenter()
{
}

LocalStreamCenter::~LocalStreamCenter()
{
}

bool LocalStreamCenter::RegisterStream(const string& app, const string& stream, MediaPublisher* media_publisher)
{
    if (app.empty() || stream.empty())
    {
        return false;
    }

    auto& stream_protocol_ = app_stream_publisher_[app];

    if (stream_protocol_.find(stream) != stream_protocol_.end())
    {
        cout << LMSG << "stream:" << stream << " already registered" << endl;
        return false;
    }

    stream_protocol_.insert(make_pair(stream, media_publisher));
    cout << LMSG << "register app:" << app << ", stream:" << stream << endl;

    return true;
}

bool LocalStreamCenter::UnRegisterStream(const string& app, const string& stream, MediaPublisher* media_publisher)
{
    auto iter_app = app_stream_publisher_.find(app);

    if (iter_app == app_stream_publisher_.end())
    {
        return false;
    }

    auto iter_stream = iter_app->second.find(stream);

    if (iter_stream == iter_app->second.end())
    {
        return false;
    }

    iter_app->second.erase(iter_stream);

    if (iter_app->second.empty())
    {
        app_stream_publisher_.erase(iter_app);
    }

    cout << LMSG << "unregister app:" << app << ", stream:" << stream << endl;

    return true;
}

MediaPublisher* LocalStreamCenter::GetMediaPublisherByAppStream(const string& app, const string& stream)
{
    auto iter_app = app_stream_publisher_.find(app);

    if (iter_app == app_stream_publisher_.end())
    {
        return NULL;
    }

    auto iter_stream = iter_app->second.find(stream);

    if (iter_stream == iter_app->second.end())
    {
        return NULL;
    }

    return iter_stream->second;
}

bool LocalStreamCenter::IsAppStreamExist(const string& app, const string& stream)
{
    auto iter_app = app_stream_publisher_.find(app);

    if (iter_app == app_stream_publisher_.end())
    {
        return false;
    }

    auto iter_stream = iter_app->second.find(stream);

    if (iter_stream == iter_app->second.end())
    {
        return false;
    }

    return true;
}

MediaPublisher* LocalStreamCenter::_DebugGetRandomMediaPublisher(string& app, string& stream)
{
    if (app_stream_publisher_.empty())
    {
        return NULL;
    }

    auto iter = app_stream_publisher_.begin();

    if (iter->second.empty())
    {
        return NULL;
    }

    auto iter_ret = iter->second.begin();

    app = iter->first;
    stream = iter_ret->first;

    return iter_ret->second;
}
