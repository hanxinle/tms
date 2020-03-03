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

enum HEVCNALUnitType 
{
    H265NalType_TRAIL_N    = 0,
    H265NalType_TRAIL_R    = 1,
    H265NalType_TSA_N      = 2,
    H265NalType_TSA_R      = 3,
    H265NalType_STSA_N     = 4,
    H265NalType_STSA_R     = 5,
    H265NalType_RADL_N     = 6,
    H265NalType_RADL_R     = 7,
    H265NalType_RASL_N     = 8,
    H265NalType_RASL_R     = 9,
    H265NalType_VCL_N10    = 10, 
    H265NalType_VCL_R11    = 11, 
    H265NalType_VCL_N12    = 12, 
    H265NalType_VCL_R13    = 13, 
    H265NalType_VCL_N14    = 14, 
    H265NalType_VCL_R15    = 15, 
    H265NalType_BLA_W_LP   = 16, 
    H265NalType_BLA_W_RADL = 17, 
    H265NalType_BLA_N_LP   = 18, 
    H265NalType_IDR_W_RADL = 19, 
    H265NalType_IDR_N_LP   = 20, 
    H265NalType_CRA_NUT    = 21, 
    H265NalType_IRAP_VCL22 = 22, 
    H265NalType_IRAP_VCL23 = 23, 
    H265NalType_RSV_VCL24  = 24, 
    H265NalType_RSV_VCL25  = 25, 
    H265NalType_RSV_VCL26  = 26, 
    H265NalType_RSV_VCL27  = 27, 
    H265NalType_RSV_VCL28  = 28, 
    H265NalType_RSV_VCL29  = 29, 
    H265NalType_RSV_VCL30  = 30, 
    H265NalType_RSV_VCL31  = 31, 
    H265NalType_VPS        = 32, 
    H265NalType_SPS        = 33, 
    H265NalType_PPS        = 34, 
    H265NalType_AUD        = 35, 
    H265NalType_EOS_NUT    = 36, 
    H265NalType_EOB_NUT    = 37, 
    H265NalType_FD_NUT     = 38, 
    H265NalType_SEI_PREFIX = 39, 
    H265NalType_SEI_SUFFIX = 40, 
    H265NalType_RSV_NVCL41 = 41, 
    H265NalType_RSV_NVCL42 = 42, 
    H265NalType_RSV_NVCL43 = 43, 
    H265NalType_RSV_NVCL44 = 44, 
    H265NalType_RSV_NVCL45 = 45, 
    H265NalType_RSV_NVCL46 = 46, 
    H265NalType_RSV_NVCL47 = 47, 
    H265NalType_UNSPEC48   = 48, 
    H265NalType_UNSPEC49   = 49, 
    H265NalType_UNSPEC50   = 50, 
    H265NalType_UNSPEC51   = 51, 
    H265NalType_UNSPEC52   = 52, 
    H265NalType_UNSPEC53   = 53, 
    H265NalType_UNSPEC54   = 54, 
    H265NalType_UNSPEC55   = 55, 
    H265NalType_UNSPEC56   = 56, 
    H265NalType_UNSPEC57   = 57,
	H265NalType_UNSPEC58   = 58,
    H265NalType_UNSPEC59   = 59,
    H265NalType_UNSPEC60   = 60,
    H265NalType_UNSPEC61   = 61,
    H265NalType_UNSPEC62   = 62,
    H265NalType_UNSPEC63   = 63,
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
