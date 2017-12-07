#include "http_flv_protocol.h"
#include "media_publisher.h"
#include "rtmp_protocol.h"
#include "server_protocol.h"

int MediaPublisher::OnNewRtmpPlayer(RtmpProtocol* protocol)
{
    cout << LMSG << endl;

    if (media_muxer_.HasMetaData())
    {    
        protocol->SendRtmpMessage(6, 1, kMetaData, (const uint8_t*)media_muxer_.GetMetaData().data(), media_muxer_.GetMetaData().size());
    }    

    if (media_muxer_.HasAudioHeader())
    {    
        string audio_header;
        audio_header.append(1, 0xAF);
        audio_header.append(1, 0x00);
        audio_header.append(media_muxer_.GetAudioHeader());

        protocol->SendRtmpMessage(4, 1, kAudio, (const uint8_t*)audio_header.data(), audio_header.size());
    }    

    if (media_muxer_.HasVideoHeader())
    {    
        string video_header;
        video_header.append(1, 0x17);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(1, 0x00);
        video_header.append(media_muxer_.GetVideoHeader());

        protocol->SendRtmpMessage(6, 1, kVideo, (const uint8_t*)video_header.data(), video_header.size());
    }    

    auto media_fast_out = media_muxer_.GetFastOut();

    for (const auto& payload : media_fast_out)
    {    
        protocol->SendMediaData(payload);
    }    
}

int MediaPublisher::OnNewFlvPlayer(HttpFlvProtocol* protocol)
{
    cout << LMSG << endl;

    protocol->SendFlvHeader();

    if (media_muxer_.HasMetaData())
    {    
        protocol->SendFlvMetaData(media_muxer_.GetMetaData());
    }    

    if (media_muxer_.HasVideoHeader())
    {
        protocol->SendFlvVideoHeader(media_muxer_.GetVideoHeader());
    }

    if (media_muxer_.HasAudioHeader())
    {
        protocol->SendFlvAudioHeader(media_muxer_.GetAudioHeader());
    }

    auto media_fast_out = media_muxer_.GetFastOut();

    for (const auto& payload : media_fast_out)
    {
        protocol->SendMediaData(payload);
    }
}
