#ifndef __TRANSCODER_H__
#define __TRANSCODER_H__

#include <deque>

#include "video_define.h"
#include "video_decoder.h"
#include "video_encoder.h"
#include "video_scale.h"

using std::deque;

class Transcoder
{
public:
    Transcoder();
    ~Transcoder();

    int StartTransThread();

    pthread_t pthread_id_;
    deque<MediaPacket> packet_queue_;
    pthread_mutex_t  packet_queue_mutex_;
    pthread_cond_t   packet_queue_cond_;

    VideoDecoder decoder_;
    vector<VideoEncoder*> encoder_vec_;
    vector<VideoScale*> scale_vec_;

    bool quit_;
};

#endif // __TRANSCODER_H__
