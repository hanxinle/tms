#include "common_define.h"
#include "local_stream_center.h"

LocalStreamCenter::LocalStreamCenter()
{
}

LocalStreamCenter::~LocalStreamCenter()
{
}

bool LocalStreamCenter::RegisterStream(const string& app, const string& stream_name, MediaPublisher* media_publisher)
{
    auto& stream_protocol_ = app_stream_publisher_[app];

    if (stream_protocol_.find(stream_name) != stream_protocol_.end())
    {
        cout << LMSG << "stream_name:" << stream_name << " already registered" << endl;
        return false;
    }

    stream_protocol_.insert(make_pair(stream_name, media_publisher));
    cout << LMSG << "register app:" << app << ", stream_name:" << stream_name << endl;

    return true;
}

bool LocalStreamCenter::UnRegisterStream(const string& app, const string& stream_name, MediaPublisher* media_publisher)
{
    auto iter_app = app_stream_publisher_.find(app);

    if (iter_app == app_stream_publisher_.end())
    {
        return false;
    }

    auto iter_stream = iter_app->second.find(stream_name);

    if (iter_stream == iter_app->second.end())
    {
        return false;
    }

    iter_app->second.erase(iter_stream);

    if (iter_app->second.empty())
    {
        app_stream_publisher_.erase(iter_app);
    }

    cout << LMSG << "unregister app:" << app << ", stream_name:" << stream_name << endl;

    return true;
}

MediaPublisher* LocalStreamCenter::GetMediaPublisherByAppStream(const string& app, const string& stream_name)
{
    auto iter_app = app_stream_publisher_.find(app);

    if (iter_app == app_stream_publisher_.end())
    {
        return NULL;
    }

    auto iter_stream = iter_app->second.find(stream_name);

    if (iter_stream == iter_app->second.end())
    {
        return NULL;
    }

    return iter_stream->second;
}

bool LocalStreamCenter::IsAppStreamExist(const string& app, const string& stream_name)
{
    auto iter_app = app_stream_publisher_.find(app);

    if (iter_app == app_stream_publisher_.end())
    {
        return false;
    }

    auto iter_stream = iter_app->second.find(stream_name);

    if (iter_stream == iter_app->second.end())
    {
        return false;
    }

    return true;
}
