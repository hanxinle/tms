#include <deque>

#include "audio_transcode.h"
#include "video_define.h"
#include "webrtc_protocol.h"

using namespace std;

extern WebrtcProtocol* g_debug_webrtc;
extern deque<MediaPacket> g_media_queue;


AudioTransCoder::AudioTransCoder()
    :
    decode_frame_count_(0),
    media_output_(NULL)
{
}

AudioTransCoder::~AudioTransCoder()
{
}

int AudioTransCoder::InitDecoder(const string& audio_header)
{
    return audio_decoder_.Init(audio_header);
}

int AudioTransCoder::Decode(uint8_t* data, const int& size, const int64_t& pts)
{
    int got_audio = 0;
    int ret = audio_decoder_.Decode(data, size, pts, got_audio);

    if (ret > 0 && got_audio == 1)
    {
        AVFrame* decode_frame = audio_decoder_.GetDecodedFrame();

        if (decode_frame != NULL)
        {
            if (decode_frame_count_ == 0)
            {
                audio_resample_.Init(decode_frame->channel_layout, AV_CH_LAYOUT_STEREO, decode_frame->sample_rate, 48000, decode_frame->format, AV_SAMPLE_FMT_S16);
                audio_resample_.OpenPcmFd();
                audio_encoder_.Init();

                if (media_output_ != NULL)
                {
                    media_output_->InitAudioStream(audio_encoder_.GetCodecContext());
                }
            }

            int got_resample = 0;
            ret = audio_resample_.Resample(decode_frame, got_resample);

            if (ret == 0)
            {
                AVFrame* resample_frame = audio_resample_.GetResampleFrame();

                if (resample_frame != NULL)
                {
                    int got_packet = 0;
                    ret = audio_encoder_.Encode(resample_frame, got_packet);

                    if (ret == 0)
                    {
						AVPacket* encode_packet = audio_encoder_.GetEncodePacket();

                        cout << LMSG << "g_media_queue[" << g_media_queue.size() << "]:" << encode_packet->size << endl;

                        g_media_queue.emplace_back((const uint8_t*)encode_packet->data, (int)encode_packet->size, (int64_t)encode_packet->dts,
                                                   (int64_t)encode_packet->dts, (int)encode_packet->flags, MediaAudio);

                        if (media_output_ != NULL)
                        {
                            media_output_->WriteAudio(encode_packet);
                        }
                        //if (g_debug_webrtc != NULL)
                        //{   
                        //    g_debug_webrtc->SendAudioData(encode_packet->data, encode_packet->size, encode_packet->dts, encode_packet->flags);
                        //}
                    }
                }
            }
        }

        ++decode_frame_count_;
    }

    return ret;
}

int AudioTransCoder::InitResample()
{
    return 0;
}

int AudioTransCoder::Resample()
{
    return 0;
}

int AudioTransCoder::InitEncoder()
{
    return 0;
}

int AudioTransCoder::Encode()
{
    return 0;
}
