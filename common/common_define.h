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

enum H264NalType 
{
    H264NalType_SLICE           = 1,  
    H264NalType_DPA             = 2,  
    H264NalType_DPB             = 3,  
    H264NalType_DPC             = 4,  
    H264NalType_IDR_SLICE       = 5,  
    H264NalType_SEI             = 6,  
    H264NalType_SPS             = 7,  
    H264NalType_PPS             = 8,  
    H264NalType_AUD             = 9,  
    H264NalType_END_SEQUENCE    = 10, 
    H264NalType_END_STREAM      = 11, 
    H264NalType_FILLER_DATA     = 12, 
    H264NalType_SPS_EXT         = 13, 
    H264NalType_AUXILIARY_SLICE = 19, 
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
    kSrt = 3,
    kWebrtc = 4,
};

enum WebSocketProtocolDefine
{
    kWebSocketProtocolHeaderSize = 2,
};


#define  LMSG  Util::GetNowMsStr()<<" @ tms ["<<__FILE__<<"]#"<<__func__<<":"<<__LINE__<<" "
#define  TRACE "============================================================"

//#define VERBOSE Log(kLevelVerbose)
//#define DEBUG Log(kLevelDebug)

#define VERBOSE std::cout<<LMSG
#define DEBUG std::cout<<LMSG


#endif // __COMMON_DEFINE_H__
