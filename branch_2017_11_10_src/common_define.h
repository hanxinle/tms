#ifndef __COMMON_DEFINE_H__
#define __COMMON_DEFINE_H__

#define CRLF "\r\n"

#include "log.h"

enum RetCode
{
    kError = -1,
    kSuccess = 0,
    kClose = 1,
    kNoEnoughData = 2,
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

#define  LMSG  "["<<__FILE__<<"]#"<<__func__<<":"<<__LINE__<<" "
#define  TRACE "============================================================"

//#define VERBOSE Log(kLevelVerbose)
//#define DEBUG Log(kLevelDebug)

#define VERBOSE cout
#define DEBUG cout


#endif // __COMMON_DEFINE_H__
