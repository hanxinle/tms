#ifndef __RTMP_PROTOCOL_H__
#define __RTMP_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <sstream>

using std::map;
using std::string;
using std::ostringstream;

class IoBuffer;
class Socket;

enum HandShakeStatus
{
    kStatus_0 = 0,
    kStatus_1,
    kStatus_2,
    kStatus_Done,
};

enum RtmpMessageType
{
    kSetChunkSize = 1,

    kAmf3Command = 17,
    kAmf0Command = 20,
};

struct RtmpMessage
{
    RtmpMessage()
        :
        timestamp(0),
        timestamp_delta(0),
        timestamp_calc(0),
        message_length(0),
        message_type_id(0),
        message_stream_id(0),
        msg(NULL),
        len(0)
    {
    }

    string ToString()
    {
        ostringstream os;

        os << "timestamp:" << timestamp
           << ",timestamp_delta:" << timestamp_delta
           << ",timestamp_calc:" << timestamp_calc
           << ",message_length:" << message_length
           << ",message_type_id:" << (uint16_t)message_type_id
           << ",message_stream_id:" << message_stream_id
           << ",msg:" << (uint64_t)msg
           << ",len:" << len;

        return os.str();
    }

    uint32_t timestamp;
    uint32_t timestamp_delta;
    uint32_t timestamp_calc;
    uint32_t message_length;
    uint8_t  message_type_id;
    uint32_t message_stream_id;

    uint8_t* msg;
    uint32_t len;
};

class RtmpProtocol
{
public:
    RtmpProtocol(Socket* socket);
    ~RtmpProtocol();

    int Parse(IoBuffer& io_buffer);

private:
    int OnRtmpMessage(RtmpMessage& rtmp_msg);
    int SendRtmpMessage(const uint8_t& message_type_id, const uint8_t* data, const size_t& len);

private:
    Socket* socket_;
    HandShakeStatus handshake_status_;

    uint32_t in_chunk_size_;
    uint32_t out_chunk_size_;

    map<uint32_t, RtmpMessage> csid_head_;

    string app_;
    string tc_url_;
    string stream_name_;
};

#endif // __RTMP_PROTOCOL_H__
