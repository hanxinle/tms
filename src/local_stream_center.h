#ifndef __LOCAL_STREAM_CENTER_H__
#define __LOCAL_STREAM_CENTER_H__

#include <map>
#include <set>
#include <string>

class MediaCenterMgr;
class MediaPublisher;
class MediaSubscriber;

class LocalStreamCenter {
 public:
  LocalStreamCenter();
  ~LocalStreamCenter();

  MediaPublisher* GetMediaPublisherByAppStream(const std::string& app,
                                               const std::string& stream);
  bool IsAppStreamExist(const std::string& app, const std::string& stream);

  bool RegisterStream(const std::string& app, const std::string& stream,
                      MediaPublisher* media_publisher);
  bool UnRegisterStream(const std::string& app, const std::string& stream,
                        MediaPublisher* media_publisher);

  MediaPublisher* _DebugGetRandomMediaPublisher(std::string& app,
                                                std::string& stream);

 private:
  std::map<std::string, std::map<std::string, MediaPublisher*>>
      app_stream_publisher_;
};

#endif  // __LOCAL_STREAM_CENTER_H__
