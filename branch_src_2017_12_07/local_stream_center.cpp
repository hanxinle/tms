#include "common_define.h"
#include "global.h"
#include "local_stream_center.h"
#include "media_subscriber.h"
#include "media_center_mgr.h"

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

    StreamRegisterReq stream_register_req;
    StreamInfo stream_info;

    stream_info.stream_name = stream_name;
    stream_info.app = app;

    stream_register_req.stream_infos.push_back(stream_info);
    stream_register_req.req_time = Util::GetNowMs();
    stream_register_req.role = MASTER;
    stream_register_req.node_info = g_node_info;

    ostringstream os;

    stream_register_req.Dump(os);

    cout << LMSG << os.str() << endl;

    g_media_center_mgr->SendAll(stream_register_req);

    auto pending_subscriber = GetAppStreamPendingSubscriber(app, stream_name);

    for (auto& pending : pending_subscriber)
    {
        pending->OnPendingArrive();
    }


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

bool LocalStreamCenter::AddAppStreamPendingSubscriber(const string& app, const string& stream, MediaSubscriber* media_subscriber)
{
    return app_stream_pending_subscriber_[app][stream].insert(media_subscriber).second;
}

set<MediaSubscriber*> LocalStreamCenter::GetAppStreamPendingSubscriber(const string& app, const string& stream)
{
    set<MediaSubscriber*> ret;

    auto iter_app = app_stream_pending_subscriber_.find(app);

    if (iter_app == app_stream_pending_subscriber_.end())
    {
        return ret;
    }

    auto iter_stream = iter_app->second.find(stream);

    if (iter_stream == iter_app->second.end())
    {
        return ret;
    }

    ret = iter_stream->second;

    return ret;
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
