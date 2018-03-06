#ifndef __AUDIO_TRANS_CODER_H__
#define __AUDIO_TRANS_CODER_H__

#include "audio_decoder.h"
#include "audio_encoder.h"
#include "audio_resample.h"

class AudioTransCoder
{
public:
    AudioTransCoder();
    ~AudioTransCoder();

    int InitDecoder(const string& audio_header);
    int Decode(uint8_t* data, const int& size, const int64_t& pts);

    int InitResample();
    int Resample();

    int InitEncoder();
    int Encode();

private:
    AudioDecoder    audio_decoder_;
    AudioEncoder    audio_encoder_;
    AudioResample   audio_resample_;

    uint64_t decode_frame_count_;
};

#endif // __AUDIO_TRANS_CODER_H__
