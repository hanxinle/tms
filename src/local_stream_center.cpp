#include "local_stream_center.h"
#include "common_define.h"
#include "global.h"
#include "media_subscriber.h"

LocalStreamCenter::LocalStreamCenter() {}

LocalStreamCenter::~LocalStreamCenter() {}

bool LocalStreamCenter::RegisterStream(const std::string& app,
                                       const std::string& stream,
                                       MediaPublisher* media_publisher) {
  if (app.empty() || stream.empty()) {
    return false;
  }

  auto& stream_protocol_ = app_stream_publisher_[app];

  if (stream_protocol_.find(stream) != stream_protocol_.end()) {
    std::cout << LMSG << "stream:" << stream << " already registered"
              << std::endl;
    return false;
  }

  stream_protocol_.insert(make_pair(stream, media_publisher));
  std::cout << LMSG << "register app:" << app << ", stream:" << stream
            << std::endl;

  return true;
}

bool LocalStreamCenter::UnRegisterStream(const std::string& app,
                                         const std::string& stream,
                                         MediaPublisher* media_publisher) {
  auto iter_app = app_stream_publisher_.find(app);

  if (iter_app == app_stream_publisher_.end()) {
    return false;
  }

  auto iter_stream = iter_app->second.find(stream);

  if (iter_stream == iter_app->second.end()) {
    return false;
  }

  iter_app->second.erase(iter_stream);

  if (iter_app->second.empty()) {
    app_stream_publisher_.erase(iter_app);
  }

  std::cout << LMSG << "unregister app:" << app << ", stream:" << stream
            << std::endl;

  return true;
}

MediaPublisher* LocalStreamCenter::GetMediaPublisherByAppStream(
    const std::string& app, const std::string& stream) {
  auto iter_app = app_stream_publisher_.find(app);

  if (iter_app == app_stream_publisher_.end()) {
    return NULL;
  }

  auto iter_stream = iter_app->second.find(stream);

  if (iter_stream == iter_app->second.end()) {
    return NULL;
  }

  return iter_stream->second;
}

bool LocalStreamCenter::IsAppStreamExist(const std::string& app,
                                         const std::string& stream) {
  auto iter_app = app_stream_publisher_.find(app);

  if (iter_app == app_stream_publisher_.end()) {
    return false;
  }

  auto iter_stream = iter_app->second.find(stream);

  if (iter_stream == iter_app->second.end()) {
    return false;
  }

  return true;
}

MediaPublisher* LocalStreamCenter::_DebugGetRandomMediaPublisher(
    std::string& app, std::string& stream) {
  if (app_stream_publisher_.empty()) {
    return NULL;
  }

  auto iter = app_stream_publisher_.begin();

  if (iter->second.empty()) {
    return NULL;
  }

  auto iter_ret = iter->second.begin();

  app = iter->first;
  stream = iter_ret->first;

  return iter_ret->second;
}
