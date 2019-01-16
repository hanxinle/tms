#ifndef __COMMON_DEFINE_H__
#define __COMMON_DEFINE_H__

#define CRLF "\r\n"

#include "log.h"
#include "util.h"

#define UNUSED(u) (void)u

enum RetCode
{
    kError = -1,
    kSuccess = 0,
    kClose = 1,
    kNoEnoughData = 2,
    kPending = 3,
};

enum ConnectStatus
{
	kDisconnected = -1, 
    kConnecting = 0,
    kConnected = 1,
    kHandshakeing = 2,
    kHandshaked = 3,
    kDisconnecting = 4,
};

enum FrameType
{
    kUnknownFrame= -1, 
    kIframe = 1,
    kBframe = 2,
    kPframe = 3,
};

enum PayloadType
{
    kUnknownPayload = -1,
    kVideoPayload = 1,
    kAudioPayload = 2,
};

enum VideoCodec
{
    kAVC = 7,
    kHEVC = 12,
};

enum AudioCodec
{
    kAAC = 0x0F,
};

enum LogLevel
{
    kLevelVerbose = 0,
    kLevelDebug   = 1,
    kLevelInfo    = 2,
    kLevelNotice  = 3,
    kLevelWarning = 4,
    kLevelError   = 5,
    kLevelFatal   = 6,
};

enum SubscriberType
{
    kRtmp = 0,
    kHttpFlv = 1,
    kHttpHls = 2,
    kTcpServer = 3,
    kWebRtc = 4,
};

enum ServerProtocolDefine
{
    kServerProtocolHeaderSize = 4,
};

enum WebSocketProtocolDefine
{
    kWebSocketProtocolHeaderSize = 2,
};

enum ProtocolId
{
    kMedia = 1,
    kSetApp = 2,
    kSetStreamName = 3,
    kPullAppStream = 4,
};

// mask
const uint32_t kPayloadTypeMask = 0xC0000000;

const uint32_t kFrameTypeMask = 0x38000000;

// type
const uint32_t kVideoTypeValue = 0x40000000;
const uint32_t kAudioTypeValue = 0x80000000;
const uint32_t kMetaTypeValue = 0xC0000000;

const uint32_t kIFrameTypeValue = 0x08000000;
const uint32_t kPFrameTypeValue = 0x10000000;
const uint32_t kBFrameTypeValue = 0x18000000;
const uint32_t kHeaderFrameTypeValue = 0x20000000;

#define MaskVideo(Mask) Mask|=kVideoTypeValue
#define MaskAudio(Mask) Mask|=kAudioTypeValue
#define MaskMeta(Mask) Mask|=kMetaTypeValue
#define MaskIFrame(Mask)    Mask|=kIFrameTypeValue
#define MaskPFrame(Mask)    Mask|=kPFrameTypeValue
#define MaskBFrame(Mask)    Mask|=kBFrameTypeValue
#define MaskHeaderFrame(Mask) Mask |= kHeaderFrameTypeValue

inline bool IsVideo(const uint32_t& mask)  { return (mask & kPayloadTypeMask) == kVideoTypeValue; }
inline bool IsAudio(const uint32_t& mask)  { return (mask & kPayloadTypeMask) == kAudioTypeValue; }
inline bool IsMetaData(const uint32_t& mask)  { return (mask & kPayloadTypeMask) == kMetaTypeValue; }
inline bool IsIFrame(const uint32_t& mask) { return (mask & kFrameTypeMask)   == kIFrameTypeValue; }
inline bool IsBFrame(const uint32_t& mask) { return (mask & kFrameTypeMask)   == kBFrameTypeValue; }
inline bool IsPFrame(const uint32_t& mask) { return (mask & kFrameTypeMask)   == kPFrameTypeValue; }
inline bool IsHeaderFrame(const uint32_t& mask) { return (mask & kFrameTypeMask)   == kHeaderFrameTypeValue; }

#define  LMSG  Util::GetNowMsStr()<<" @ Trs ["<<__FILE__<<"]#"<<__func__<<":"<<__LINE__<<" "
#define  TRACE "============================================================"

//#define VERBOSE Log(kLevelVerbose)
//#define DEBUG Log(kLevelDebug)

#define VERBOSE cout<<LMSG
#define DEBUG cout<<LMSG


#endif // __COMMON_DEFINE_H__
