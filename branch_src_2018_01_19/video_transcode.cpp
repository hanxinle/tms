#include <deque>

#include "video_define.h"
#include "video_transcode.h"
#include "webrtc_protocol.h"

using namespace std;

extern WebrtcProtocol* g_debug_webrtc;
extern deque<MediaPacket> g_media_queue;

VideoTransCoder::VideoTransCoder()
    :
    decode_frame_count_(0),
    media_output_(NULL)
{
}

VideoTransCoder::~VideoTransCoder()
{
}

int VideoTransCoder::InitDecoder(const string& video_header)
{
    return video_decoder_.Init(video_header);
}

int VideoTransCoder::Decode(uint8_t* data, const int& size, const int64_t& dts, const int64_t& pts, int& got_picture)
{
    int ret = video_decoder_.Decode(data, size, dts, pts, got_picture);

    if (ret > 0 && got_picture == 1)
    {
        AVFrame* decode_frame = video_decoder_.GetDecodedFrame();
        if (decode_frame != NULL)
        {
            if (decode_frame_count_ == 0)
            {
#if 0
                video_scale_.Init(decode_frame, decode_frame->width, decode_frame->height);
                //video_encoder_.Init("libvpx-vp9", decode_frame->width, decode_frame->height, 30, 4000);
                video_encoder_.Init("libvpx", decode_frame->width, decode_frame->height, 30, 4000);
#else
                video_scale_.Init(decode_frame, 1440, 900);
                //video_encoder_.Init("libvpx-vp9", decode_frame->width, decode_frame->height, 30, 4000);
                video_encoder_.Init("libvpx", 1440, 900, 30, 1500*1000, video_decoder_.GetCodecContext());
                if (media_output_ != NULL)
                {
                    media_output_->InitVideoStream(video_encoder_.GetCodecContext());
                }
#endif
            }

#if 1
            if (video_scale_.Scale(decode_frame) == 0)
#else
            if (true)
#endif
            {
#if 1
                AVFrame* scale_frame = video_scale_.GetScaleFrame();
#else
                AVFrame* scale_frame = decode_frame;
#endif
                
                if (scale_frame != NULL)
                {
                    int got_packet = 0;
                    if (video_encoder_.Encode(scale_frame, got_packet) == 0)
                    {
                        AVPacket* encode_packet = video_encoder_.GetEncodePacket();

                        cout << LMSG << "g_media_queue[" << g_media_queue.size() << "]:" << encode_packet->size << endl;

                        g_media_queue.emplace_back((const uint8_t*)encode_packet->data, (int)encode_packet->size, (int64_t)encode_packet->dts, 
                                                   (int64_t)encode_packet->dts, (int)encode_packet->flags, MediaVideo);
                        if (media_output_ != NULL)
                        {
                            media_output_->WriteVideo(encode_packet);
                        }
                        //if (g_debug_webrtc != NULL)
                        //{
                        //    g_debug_webrtc->SendVideoData(encode_packet->data, encode_packet->size, encode_packet->dts, encode_packet->flags);
                        //}
                    }
                }
            }
        }

        ++decode_frame_count_;
    }

    return ret;
}

int VideoTransCoder::InitScale(const AVFrame* src_frame, const int& dst_width, const int& dst_height)
{
    return video_scale_.Init(src_frame, dst_width, dst_height);
}

int VideoTransCoder::Scale()
{
    return 0;
}

int VideoTransCoder::InitEncoder(const string& encoder_name, const int& width, const int& height, const int& fps, const int& bitrate)
{
    return video_encoder_.Init(encoder_name, width, height, fps, bitrate, video_decoder_.GetCodecContext());
}

int VideoTransCoder::Encode(const AVFrame* frame, int& got_packet)
{
    return video_encoder_.Encode(frame, got_packet);
}
