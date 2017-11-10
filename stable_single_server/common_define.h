#ifndef __COMMON_DEFINE_H__
#define __COMMON_DEFINE_H__

#define CRLF "\r\n"

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

#define  LMSG  "["<<__FILE__<<"]#"<<__func__<<":"<<__LINE__<<" "
#define  TRACE "============================================================"

#endif // __COMMON_DEFINE_H__
