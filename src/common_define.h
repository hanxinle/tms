#ifndef __COMMON_DEFINE_H__
#define __COMMON_DEFINE_H__

enum RetCode
{
    kError = -1,
    kSuccess = 0,
    kClose = 1,
    kNoEnoughData = 2,
};

#define  LMSG  "["<<__FILE__<<"]#"<<__func__<<":"<<__LINE__<<" "
#define  TRACE "============================================================"

#endif // __COMMON_DEFINE_H__
