#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "bit_buffer.h"
#include "common_define.h"
#include "ref_ptr.h"
#include "ts_reader.h"
#include "util.h"

const int kTsSegmentFixedSize = 188;
const uint8_t kTsHeaderSyncByte = 0x47;

/* stream_id
    10111100 //1 program_stream_map
    10111101 //2 private_stream_1
    10111110 //padding_stream
    10111111 //3 private_stream_2
    110xxxxx //ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3 audio stream number x xxxx
    1110xxxx //ITU-T Rec. H.262 | ISO/IEC 13818-2 or ISO/IEC 11172-2 or ISO/IEC 14496-2 video stream number xxxx
    11110000 //3 ECM_stream
    11110001 //3 EMM_stream
    11110010 //5 ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A or ISO/IEC 13818- 6_DSMCC_stream
    11110011 //2 ISO/IEC_13522_stream
    11110100 //6 ITU-T Rec. H.222.1 type A
    11110101 //6 ITU-T Rec. H.222.1 type B
    11110110 //6 ITU-T Rec. H.222.1 type C
    11110111 //6 ITU-T Rec. H.222.1 type D
    11111000 //6 ITU-T Rec. H.222.1 type E
    11111001 //7 ancillary_stream
    11111010 //ISO/IEC14496-1_SL-packetized_stream
    11111011 //ISO/IEC14496-1_FlexMux_stream
    11111100 //… 1111 1110 reserved data stream
    11111111 //4 program_stream_directory
*/

const uint8_t kProgramStreamMap                = 188;
const uint8_t kPrivateStream1                  = 189;
const uint8_t kPaddingStream                   = 190;
const uint8_t kPrivateStream2                  = 191;
const uint8_t kAudioStreamBegin                = 192;
const uint8_t kAudioStreamEnd                  = 223;
const uint8_t kVideoStreamBegin                = 224;
const uint8_t kVideoStreamEnd                  = 239;
const uint8_t kECMStream                       = 240;
const uint8_t kEMMStream                       = 241;
const uint8_t kDSMCCStream                     = 242;
const uint8_t kIEC_13522_stream                = 243;
const uint8_t kH_222_1_A                       = 244;
const uint8_t kH_222_1_B                       = 245;
const uint8_t kH_222_1_C                       = 246;
const uint8_t kH_222_1_D                       = 247;
const uint8_t kH_222_1_E                       = 248;
const uint8_t kAncillaryStream                 = 249;
const uint8_t kIEC14496_1_SL_packetized_stream = 250;
const uint8_t kIEC14496_1_FlexMux_stream       = 251;
const uint8_t kProgramStreamDirector           = 255;

bool IsStreamIdVideo(const uint8_t& stream_id)
{
    return stream_id >= kVideoStreamBegin && stream_id <= kVideoStreamEnd;
}

bool IsStreamIdAudio(const uint8_t& stream_id)
{
    return stream_id >= kAudioStreamBegin && stream_id <= kAudioStreamEnd;
}

const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

static std::map<int, int> kSampingFrequency =
{
    {0, 96000}, {1, 88200}, {2, 64000}, {3,  48000}, {4, 44100}, {5,  32000}, {6, 24000},
    {7, 22050}, {8, 16000}, {9, 12000}, {10, 11025}, {11, 8000}, {12, 7350}
};

static std::map<int, int> kChannelConfigurations = 
{
    {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 8}
};

int GetSampleRate(const int& sampling_frequency_index)
{
    auto iter = kSampingFrequency.find(sampling_frequency_index);

    if (iter == kSampingFrequency.end())
    {
        return -1;
    }

    return iter->second;
}

int GetChannles(const int& channel_configuration)
{
    auto iter = kChannelConfigurations.find(channel_configuration);
    if (iter == kChannelConfigurations.end())
    {
        return -1;
    }

    return iter->second;
}

static bool IsVideoStreamType(const uint8_t& stream_type)
{
    return stream_type == 0x1B;
}

static bool IsAudioStreamType(const uint8_t& stream_type)
{
    return stream_type == 0x0F;
}

TsReader::TsReader()
    : pmt_pid_(0xFFFF)
    , video_pid_(0xFFFF)
    , audio_pid_(0xFFFF)
    , video_dump_fd_(-1)
    , audio_dump_fd_(-1)
{
}

TsReader::~TsReader()
{
}

int TsReader::ParseTs(const uint8_t* data, const int& len)
{
    if (len % kTsSegmentFixedSize != 0)
    {
        std::cout << LMSG << "ts len=" << len << ", invalid" << std::endl;
        return -1;
    }

    int l = len;
    const uint8_t* buf = data;
    while (l >= kTsSegmentFixedSize && buf[0] == kTsHeaderSyncByte)
    {
        if (ParseTsSegment(buf, kTsSegmentFixedSize) != 0)
        {
            return -1;
        }
        
        l -= kTsSegmentFixedSize;
        buf += kTsSegmentFixedSize;
    }

    return 0;
}

int TsReader::ParseTsSegment(const uint8_t* data, const int& len)
{
    if (data == NULL || len != kTsSegmentFixedSize || data[0] != kTsHeaderSyncByte)
    {
        return -1;
    }

    std::ostringstream os;
    BitBuffer bit_buffer(data, len);

    uint8_t sync_byte = 0;
    bit_buffer.GetBits(8, sync_byte);

    uint8_t transport_error_indicator = 0;
    bit_buffer.GetBits(1, transport_error_indicator);

    uint8_t payload_unit_start_indicator = 0;
    bit_buffer.GetBits(1, payload_unit_start_indicator);

    os << "payload_unit_start_indicator=" << (int)payload_unit_start_indicator;

    uint8_t transport_priority = 0;
    bit_buffer.GetBits(1, transport_priority);

    uint16_t pid = 0xFFFF;
    bit_buffer.GetBits(13, pid);

    os << ",pid=" << pid;

    uint8_t transport_scambling_control = 0;
    bit_buffer.GetBits(2, transport_scambling_control);

    uint8_t adaptation_field_control = 0;
    bit_buffer.GetBits(2, adaptation_field_control);

    uint8_t continuity_counter = 0xFF;
    bit_buffer.GetBits(4, continuity_counter);

    if (payload_unit_start_indicator == 0x01 && (pid == 0 || pid == pmt_pid_))
    {
        os << ",skip one bytes";
        uint8_t skip;
        bit_buffer.GetBits(8, skip);
    }

    os << ",adaptation_field_control=" << (int)adaptation_field_control;
    os << ",continuity_counter=" << (int)continuity_counter;

    if (adaptation_field_control == 2 || adaptation_field_control == 3)
    {
        ParseAdaptation(bit_buffer);
    }

    std::cout << LMSG << os.str() << std::endl;

    if (pid == 0x0000)
    {
        return ParsePAT(bit_buffer);
    }
    else if (pid == pmt_pid_)
    {
        return ParsePMT(bit_buffer);
    }
    else if (pid == audio_pid_)
    {
        if (payload_unit_start_indicator)
        {
            ParsePES(audio_pes_);
        }
        return CollectAudioPES(bit_buffer);
    }
    else if (pid == video_pid_)
    {
        if (payload_unit_start_indicator)
        {
            ParsePES(video_pes_);
        }
        return CollectVideoPES(bit_buffer);
    }

    return 0;
}

int TsReader::ParseAdaptation(BitBuffer& bit_buffer)
{
    std::ostringstream os;

    uint8_t adaptation_field_length = 0;
    bit_buffer.GetBits(8, adaptation_field_length);

    os << "adaptation_field_length=" << (int)adaptation_field_length;

    if (adaptation_field_length == 0)
    {
        return 0;
    }

    int begin = bit_buffer.BytesLeft();

    uint8_t discontinuity_indicator = 0;
    bit_buffer.GetBits(1, discontinuity_indicator);

	uint8_t random_access_indicator = 0;
    bit_buffer.GetBits(1, random_access_indicator);

	uint8_t elementary_stream_priority_indicator = 0;
    bit_buffer.GetBits(1, elementary_stream_priority_indicator);

	uint8_t PCR_flag = 0;
    bit_buffer.GetBits(1, PCR_flag);

	uint8_t OPCR_flag = 0;
    bit_buffer.GetBits(1, OPCR_flag);

	uint8_t splicing_point_flag = 0;
    bit_buffer.GetBits(1, splicing_point_flag);

	uint8_t transport_private_data_flag = 0;
    bit_buffer.GetBits(1, transport_private_data_flag);

	uint8_t adaptation_field_extension_flag = 0;
    bit_buffer.GetBits(1, adaptation_field_extension_flag);

    if (PCR_flag == 0x01)
    {
		uint64_t program_clock_reference_base = 0;
        bit_buffer.GetBits(33, program_clock_reference_base);
        uint8_t reserved;
        bit_buffer.GetBits(6, reserved);
        uint16_t program_clock_reference_extension;
        bit_buffer.GetBits(9, program_clock_reference_extension);

        os << ",pcr=" << program_clock_reference_base * 300 + program_clock_reference_extension;
    }
    if (OPCR_flag == 0x01)
    {
		uint64_t original_program_clock_reference_base = 0;
        bit_buffer.GetBits(33, original_program_clock_reference_base);
		uint8_t reserved;
        bit_buffer.GetBits(6, reserved);
		uint16_t original_program_clock_reference_extension;
        bit_buffer.GetBits(9, original_program_clock_reference_extension);
    }
    if (splicing_point_flag == 0x01)
    {
		uint8_t splice_countdown = 0;
        bit_buffer.GetBits(8, splice_countdown);
    }
    if (transport_private_data_flag == 0x01)
    {
		uint8_t transport_private_data_length = 0;
        bit_buffer.GetBits(8, transport_private_data_length);
        os << ",transport_private_data_length=" << (int)transport_private_data_length;
		for (int i = 0; i < transport_private_data_length; i++) 
        {
			uint8_t private_data_byte;
            bit_buffer.GetBits(8, private_data_byte);
		}
    }

	if (adaptation_field_extension_flag == 0x01)
	{
		uint8_t adaptation_field_extension_length;
        bit_buffer.GetBits(8, adaptation_field_extension_length);

        int begin = bit_buffer.BytesLeft();

		uint8_t ltw_flag;
        bit_buffer.GetBits(1, ltw_flag);

		uint8_t piecewise_rate_flag;
        bit_buffer.GetBits(1, piecewise_rate_flag);

		uint8_t seamless_splice_flag;
        bit_buffer.GetBits(1, seamless_splice_flag);

		uint8_t reserved;
        bit_buffer.GetBits(5, reserved);

		if (ltw_flag == 0x01) 
        {
			uint8_t ltw_valid_flag;
            bit_buffer.GetBits(1, ltw_valid_flag);

			uint16_t ltw_offset;
            bit_buffer.GetBits(15, ltw_offset);
		}

		if (piecewise_rate_flag == 0x01) 
        {
		    uint8_t reserved;
            bit_buffer.GetBits(2, reserved);

		    uint32_t piecewise_rate;
            bit_buffer.GetBits(22, piecewise_rate);
		}

		if (seamless_splice_flag == 0x01) 
        {
			uint8_t splice_type;
            bit_buffer.GetBits(4, splice_type);

			uint8_t DTS_next_AU32_30;
            bit_buffer.GetBits(3, DTS_next_AU32_30);

			uint8_t marker_bit = 0;
            bit_buffer.GetBits(1, marker_bit);

			uint16_t DTS_next_AU29_15;
            bit_buffer.GetBits(15, DTS_next_AU29_15);

			uint16_t DTS_next_AU14_0;
            bit_buffer.GetBits(15, DTS_next_AU14_0);

            bit_buffer.GetBits(1, marker_bit);
		}

        int end = bit_buffer.BytesLeft();
        adaptation_field_extension_length -= (begin - end);

        os << ",adaptation_field_extension_length=" << (int)adaptation_field_extension_length;
		for (int i = 0; i < adaptation_field_extension_length; i++) 
        {
            bit_buffer.GetBits(8, reserved);
		}
    }

    int end = bit_buffer.BytesLeft();
    adaptation_field_length -= (begin - end);
    os << ",adaptation_field_length=" << (int)adaptation_field_length;

    // FIXME: skip stuffing_byte
    for (int i = 0; i < adaptation_field_length; i++) 
    {
        uint8_t stuffing_byte = 0;
        bit_buffer.GetBits(8, stuffing_byte);
        assert(stuffing_byte == 0xFF);
    }

    std::cout << LMSG << os.str() << std::endl;

    return 0;
}

int TsReader::ParsePAT(BitBuffer& bit_buffer)
{
    std::ostringstream os;

    uint8_t table_id;
    bit_buffer.GetBits(8, table_id);

    os << "table_id=" << (int)table_id;

    uint8_t section_syntax_indicator;
    bit_buffer.GetBits(1, section_syntax_indicator);

    uint8_t zero;
    bit_buffer.GetBits(1, zero);

    uint8_t reserved;
    bit_buffer.GetBits(2, reserved);

    uint16_t section_length;
    bit_buffer.GetBits(12, section_length);

    os << ",section_length=" << section_length;

    uint16_t transport_stream_id;
    bit_buffer.GetBits(16, transport_stream_id);

    bit_buffer.GetBits(2, reserved);

    uint8_t version_number;
    bit_buffer.GetBits(5, version_number);

    uint8_t current_next_indicator;
    bit_buffer.GetBits(1, current_next_indicator);

    uint8_t section_number;
    bit_buffer.GetBits(8, section_number);

    uint8_t last_section_number;
    bit_buffer.GetBits(8, last_section_number);

    //for (int i = 0; i < N; i++) 
    {
        uint16_t program_number = 0;
        bit_buffer.GetBits(16, program_number);

        os << ",program_number=" << program_number;

        uint8_t reserved;
        bit_buffer.GetBits(3, reserved);

        if (program_number == 0x00) 
        {
            uint16_t network_PID;
            bit_buffer.GetBits(13, network_PID);
        }
        else 
        {
            uint16_t program_map_PID;
            bit_buffer.GetBits(13, program_map_PID);
            pmt_pid_ = program_map_PID;
            os << ",pmt_pid_=" << pmt_pid_;
        }
    }
    uint32_t CRC_32;
    bit_buffer.GetBits(32, CRC_32);

    std::cout << LMSG << os.str() << std::endl;

    return 0;
}

int TsReader::ParsePMT(BitBuffer& bit_buffer)
{
    std::ostringstream os;

	uint8_t table_id = 0;
    bit_buffer.GetBits(8, table_id);

    os << "table_id=" << (int)table_id;

	uint8_t section_syntax_indicator = 0;
    bit_buffer.GetBits(1, section_syntax_indicator);

	uint8_t zero = 0;
    bit_buffer.GetBits(1, zero);

	uint8_t reserved = 0;
    bit_buffer.GetBits(2, reserved);

	uint16_t section_length = 0;
    bit_buffer.GetBits(12, section_length);

    os << ",section_length=" << section_length;

	uint16_t program_number = 0;
    bit_buffer.GetBits(16, program_number);

    bit_buffer.GetBits(2, reserved);

	uint8_t version_number = 0;
    bit_buffer.GetBits(5, version_number);

	uint8_t current_next_indicator = 0;
    bit_buffer.GetBits(1, current_next_indicator);

	uint8_t section_number = 0;
    bit_buffer.GetBits(8, section_number);

	uint8_t last_section_number = 0;
    bit_buffer.GetBits(8, last_section_number);

    bit_buffer.GetBits(3, reserved);

	uint16_t PCR_PID = 0;
    bit_buffer.GetBits(13, PCR_PID);
    os << ",PCR_PID=" << PCR_PID;

    bit_buffer.GetBits(4, reserved);

	uint16_t program_info_length = 0;
    bit_buffer.GetBits(12, program_info_length);
    os << ",program_info_length=" << program_info_length;

    section_length -= 9;
    int es_info_bytes = section_length - 4;

	for (int i = 0; i < program_info_length; i++) 
    {
	    //descriptor();
	}

    os << ",es_info_bytes=" << es_info_bytes;

    while (bit_buffer.BytesLeft() > 4 && es_info_bytes > 0)
    {
        int begin = bit_buffer.BytesLeft();
	    uint8_t stream_type = 0;
        bit_buffer.GetBits(8, stream_type);

	    uint8_t reserved = 0;
        bit_buffer.GetBits(3, reserved);

	    uint16_t elementary_PID = 0;
        bit_buffer.GetBits(13, elementary_PID);

        if (IsVideoStreamType(stream_type))
        {
            video_pid_ = elementary_PID;
            os << ",video_pid_=" << video_pid_;
        }
        else if (IsAudioStreamType(stream_type))
        {
            audio_pid_ = elementary_PID;
            os << ",audio_pid_=" << audio_pid_;
        }

        bit_buffer.GetBits(4, reserved);

	    uint16_t ES_info_length = 0;
        bit_buffer.GetBits(12, ES_info_length);

        os << ",stream_type=" << (int)stream_type << ", elementary_PID=" << elementary_PID << ", ES_info_length=" << ES_info_length;

	    for (int i = 0; i < ES_info_length; i++) 
        {
	        //descriptor();
	    }

        int end = bit_buffer.BytesLeft();
        es_info_bytes -= (begin - end);
        os << ",es_info_bytes=" << es_info_bytes;
	}

	uint32_t CRC_32 = 0;
    bit_buffer.GetBits(32, CRC_32);

    os << ",CRC_32=" << CRC_32;

    std::cout << LMSG << os.str() << std::endl;

    return 0;
}

int TsReader::ParsePES(std::string& pes)
{
    if (pes.empty())
    {
        return 0;
    }

    std::ostringstream os;

    BitBuffer bit_buffer((const uint8_t*)pes.data(), pes.size());

	uint32_t packet_start_code_prefix;
    bit_buffer.GetBits(24, packet_start_code_prefix);

	uint8_t stream_id;
    bit_buffer.GetBits(8, stream_id);

    os << "stream_id=" << (int)stream_id;

	uint16_t PES_packet_length;
    bit_buffer.GetBits(16, PES_packet_length);

    os << ",PES_packet_length=" << (int)PES_packet_length;

	if (stream_id != kProgramStreamMap && 
		stream_id != kPaddingStream && 
		stream_id != kPrivateStream2 && 
		stream_id != kECMStream && 
        stream_id != kEMMStream && 
		stream_id != kProgramStreamDirector && 
		stream_id != kDSMCCStream && 
		stream_id != kH_222_1_E) 
	{
	    uint8_t zero = 0;
        bit_buffer.GetBits(2, zero);

	    uint8_t PES_scrambling_control = 0;
        bit_buffer.GetBits(2, PES_scrambling_control);

	    uint8_t PES_priority = 0;
        bit_buffer.GetBits(1, PES_priority);

	    uint8_t data_alignment_indicator = 0;
        bit_buffer.GetBits(1, data_alignment_indicator);

	    uint8_t copyright = 0;
        bit_buffer.GetBits(1, copyright);

	    uint8_t original_or_copy = 0;
        bit_buffer.GetBits(1, original_or_copy);

	    uint8_t PTS_DTS_flags = 0;
        bit_buffer.GetBits(2, PTS_DTS_flags);

        os << ",PTS_DTS_flags=" << (int)PTS_DTS_flags;

	    uint8_t ESCR_flag = 0;
        bit_buffer.GetBits(1, ESCR_flag);

	    uint8_t ES_rate_flag = 0;
        bit_buffer.GetBits(1, ES_rate_flag);

	    uint8_t DSM_trick_mode_flag = 0;
        bit_buffer.GetBits(1, DSM_trick_mode_flag);

	    uint8_t additional_copy_info_flag = 0;
        bit_buffer.GetBits(1, additional_copy_info_flag);

	    uint8_t PES_CRC_flag = 0;
        bit_buffer.GetBits(1, PES_CRC_flag);

	    uint8_t PES_extension_flag = 0;
        bit_buffer.GetBits(1, PES_extension_flag);

	    uint8_t PES_header_data_length = 0;
        bit_buffer.GetBits(8, PES_header_data_length);

        os << ",PES_header_data_length=" << (int)PES_header_data_length;

        uint32_t pts = 0;
        uint32_t dts = 0;

	    if (PTS_DTS_flags == 2) 
        {
	        uint8_t zero = 0;
            bit_buffer.GetBits(4, zero);

	        uint32_t PTS_32_30 = 0;
            bit_buffer.GetBits(3, PTS_32_30);

	        uint8_t marker_bit = 0;
            bit_buffer.GetBits(1, marker_bit);

	        uint32_t PTS_29_15 = 0;
            bit_buffer.GetBits(15, PTS_29_15);

            bit_buffer.GetBits(1, marker_bit);

	        uint32_t PTS_14_0 = 0;
            bit_buffer.GetBits(15, PTS_14_0);

            pts = (PTS_32_30 << 29) | (PTS_29_15 << 15) | PTS_14_0;
            dts = pts;
            os << ",dts=pts=" << pts;

            bit_buffer.GetBits(1, marker_bit);
	    }
	    if (PTS_DTS_flags == 3) 
        {
	        uint8_t zero = 0;
            bit_buffer.GetBits(4, zero);

	        uint32_t PTS_32_30 = 0;
            bit_buffer.GetBits(3, PTS_32_30);

	        uint8_t marker_bit = 0;
            bit_buffer.GetBits(1, marker_bit);

	        uint32_t PTS_29_15 = 0;
            bit_buffer.GetBits(15, PTS_29_15);

            bit_buffer.GetBits(1, marker_bit);

	        uint32_t PTS_14_0 = 0;
            bit_buffer.GetBits(15, PTS_14_0);

            pts = (PTS_32_30 << 29) | (PTS_29_15 << 15) | PTS_14_0;
            os << ",pts=" << pts;

            bit_buffer.GetBits(1, marker_bit);

            bit_buffer.GetBits(4, zero);

	        uint32_t DTS_32_30 = 0;
            bit_buffer.GetBits(3, DTS_32_30);

            bit_buffer.GetBits(1, marker_bit);

	        uint32_t DTS_29_15 = 0;
            bit_buffer.GetBits(15, DTS_29_15);

            bit_buffer.GetBits(1, marker_bit);

	        uint32_t DTS_14_0 = 0;
            bit_buffer.GetBits(15, DTS_14_0);

            dts = (DTS_32_30 << 29) | (DTS_29_15 << 15) | DTS_14_0;
            os << ",dts=" << dts;

            bit_buffer.GetBits(1, marker_bit);
	    }
	    if (ESCR_flag == 1) 
        {
	        uint8_t reserved = 0;
            bit_buffer.GetBits(2, reserved);

	        uint8_t ESCR_base_32_30 = 0;
            bit_buffer.GetBits(3, ESCR_base_32_30);

	        uint8_t marker_bit = 0;
            bit_buffer.GetBits(1, marker_bit);

	        uint16_t ESCR_base_29_15 = 0;
            bit_buffer.GetBits(15, ESCR_base_29_15);

            bit_buffer.GetBits(1, marker_bit);

	        uint16_t ESCR_base_14_0 = 0;
            bit_buffer.GetBits(15, ESCR_base_14_0);

            bit_buffer.GetBits(1, marker_bit);

	        uint16_t ESCR_extension = 0;
            bit_buffer.GetBits(9, ESCR_extension);

            bit_buffer.GetBits(1, marker_bit);
	    }
	    if (ES_rate_flag == 1) 
        {
	        uint8_t marker_bit = 0;
            bit_buffer.GetBits(1, marker_bit);

	        uint32_t ES_rate = 0;
            bit_buffer.GetBits(22, ES_rate);

            bit_buffer.GetBits(1, marker_bit);
	    }
	    if (DSM_trick_mode_flag == 1) 
        {
	        uint8_t trick_mode_control = 0;
            bit_buffer.GetBits(3, trick_mode_control);

	        if ( trick_mode_control == 0 ) 
            {
	            uint8_t field_id = 0;
                bit_buffer.GetBits(2, field_id);

	            uint8_t intra_slice_refresh = 0;
                bit_buffer.GetBits(1, intra_slice_refresh);

	            uint8_t frequency_truncation = 0;
                bit_buffer.GetBits(2, frequency_truncation);
	        }
	        else if ( trick_mode_control == 1 ) 
            {
	            uint8_t rep_cntrl = 0;
                bit_buffer.GetBits(5, rep_cntrl);
	        }
	        else if ( trick_mode_control == 2 ) 
            {
	            uint8_t field_id = 0;
                bit_buffer.GetBits(2, field_id);

	            uint8_t reserved = 0;
                bit_buffer.GetBits(3, reserved);
	        }
	        else if ( trick_mode_control == 3 ) 
            {
	            uint8_t field_id = 0;
                bit_buffer.GetBits(2, field_id);

	            uint8_t intra_slice_refresh = 0;
                bit_buffer.GetBits(1, intra_slice_refresh);

	            uint8_t frequency_truncation = 0;
                bit_buffer.GetBits(2, frequency_truncation);
	        }
	        else if ( trick_mode_control == 4 ) 
            {
	            uint8_t rep_cntrl = 0;
                bit_buffer.GetBits(5, rep_cntrl);
	        }
	        else
            {
	            uint8_t reserved = 0;
                bit_buffer.GetBits(5, reserved);
            }
	    }
	    if (additional_copy_info_flag == 1) 
        {
	        uint8_t marker_bit = 0;
            bit_buffer.GetBits(1, marker_bit);

	        uint8_t additional_copy_info = 0;
            bit_buffer.GetBits(7, additional_copy_info);
	    }
	    if (PES_CRC_flag == 1) 
        {
	        uint16_t previous_PES_packet_CRC = 0;
            bit_buffer.GetBits(16, previous_PES_packet_CRC);
	    }
	    if (PES_extension_flag == 1) 
        {
	        uint8_t PES_private_data_flag = 0;
            bit_buffer.GetBits(1, PES_private_data_flag);

	        uint8_t pack_header_field_flag = 0;
            bit_buffer.GetBits(1, pack_header_field_flag);

	        uint8_t program_packet_sequence_counter_flag = 0;
            bit_buffer.GetBits(1, program_packet_sequence_counter_flag);

	        uint8_t P_STD_buffer_flag = 0;
            bit_buffer.GetBits(1, P_STD_buffer_flag);

	        uint8_t reserved = 0;
            bit_buffer.GetBits(3, reserved);

	        uint8_t PES_extension_flag_2 = 0;
            bit_buffer.GetBits(1, PES_extension_flag_2);

	        if (PES_private_data_flag == 1) 
            {
                bit_buffer.SkipBytes(16);
	        }
	        if (pack_header_field_flag == 1) 
            {
	            uint8_t pack_field_length = 0;
                PackHead(bit_buffer);
	        }
	        if (program_packet_sequence_counter_flag == 1) 
            {
	            uint8_t marker_bit = 0;
                bit_buffer.GetBits(1, marker_bit);

	            uint8_t program_packet_sequence_counter = 0;
                bit_buffer.GetBits(7, program_packet_sequence_counter);

                bit_buffer.GetBits(1, marker_bit);

	            uint8_t MPEG1_MPEG2_identifier = 0;
                bit_buffer.GetBits(1, MPEG1_MPEG2_identifier);

	            uint8_t original_stuff_length = 0;
                bit_buffer.GetBits(6, original_stuff_length);
	        }
	        if ( P_STD_buffer_flag == 1) 
            {
	            uint8_t zero = 0;
                bit_buffer.GetBits(2, zero);

	            uint8_t P_STD_buffer_scale = 0;
                bit_buffer.GetBits(1, P_STD_buffer_scale);

	            uint16_t P_STD_buffer_size = 0;
                bit_buffer.GetBits(13, P_STD_buffer_size);
	        }
	        if ( PES_extension_flag_2 == 1) 
            {
	            uint8_t marker_bit = 0;
                bit_buffer.GetBits(1, marker_bit);

	            uint8_t PES_extension_field_length = 0;
                bit_buffer.GetBits(7, PES_extension_field_length);

	            for (int i = 0; i < PES_extension_field_length; i++) 
                {
	                uint8_t reserved = 0;
                    bit_buffer.GetBits(8, reserved);
	            }
	        }
	    }

        if (IsStreamIdVideo(stream_id))
        {
            OpenVideoDumpFile();
            DumpVideo(bit_buffer.CurData(), bit_buffer.BytesLeft());

			OnVideo(bit_buffer, pts, dts);
        }
        else if (IsStreamIdAudio(stream_id))
        {
            OpenAudioDumpFile();
            DumpAudio(bit_buffer.CurData(), bit_buffer.BytesLeft());

            OnAudio(bit_buffer, pts);
        }
        else
        {
            os << ",no av streamid=" << (int)stream_id;
        }
	}
	else if (stream_id == kProgramStreamMap || 
             stream_id == kPrivateStream2 || 
             stream_id == kECMStream || 
             stream_id == kEMMStream || 
             stream_id == kProgramStreamDirector || 
             stream_id == kDSMCCStream ||
             stream_id == kH_222_1_E) 
    {
        //os << ",pes before stuffing_byte=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 10 ? 10 : bit_buffer.BytesLeft());
	    //for (int i = 0; i < PES_packet_length; i++) 
        //{
	    //    uint8_t PES_packet_data_byte = 0;
        //    bit_buffer.GetBits(8, PES_packet_data_byte);
	    //}
	}
	else if (stream_id == kPaddingStream) 
    {
        //os << ",pes before stuffing_byte=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 10 ? 10 : bit_buffer.BytesLeft());
	    //for (int i = 0; i < PES_packet_length; i++) 
        //{
	    //    uint8_t padding_byte = 0;
        //    bit_buffer.GetBits(8, padding_byte);
	    //}
	}

    pes.clear();

    std::cout << LMSG << os.str() << std::endl;

    return 0;
}

int TsReader::PackHead(BitBuffer& bit_buffer)
{
    std::cout << LMSG << std::endl;

	uint32_t pack_start_code;
    bit_buffer.GetBits(32, pack_start_code);

	uint8_t zero;
    bit_buffer.GetBits(2, zero);

	uint8_t system_clock_reference_base_32_30;
    bit_buffer.GetBits(3, system_clock_reference_base_32_30);

	uint8_t marker_bit;
    bit_buffer.GetBits(1, marker_bit);

	uint16_t system_clock_reference_base_29_15;
    bit_buffer.GetBits(15, system_clock_reference_base_29_15);

    bit_buffer.GetBits(1, marker_bit);

	uint16_t system_clock_reference_base_14_0;
    bit_buffer.GetBits(15, system_clock_reference_base_14_0);

    bit_buffer.GetBits(1, marker_bit);

	uint16_t system_clock_reference_extension;
    bit_buffer.GetBits(9, system_clock_reference_extension);

    bit_buffer.GetBits(1, marker_bit);

	uint32_t program_mux_rate;
    bit_buffer.GetBits(22, program_mux_rate);

    bit_buffer.GetBits(1, marker_bit);

    bit_buffer.GetBits(1, marker_bit);

	uint8_t reserved;
    bit_buffer.GetBits(5, reserved);

	uint8_t pack_stuffing_length = 0;
    bit_buffer.GetBits(3, pack_stuffing_length);

	for (int i = 0; i < pack_stuffing_length; i++) 
    {
	    uint8_t stuffing_byte;
        bit_buffer.GetBits(8, stuffing_byte);
	}

    uint32_t peekbits = 0;
    bit_buffer.GetBits(32, peekbits);
	if (peekbits == 0x000001BA) 
    {
	    SystemHeader(bit_buffer);
	}

    return 0;
}

int TsReader::SystemHeader(BitBuffer& bit_buffer) 
{
    std::cout << LMSG << std::endl;

    uint32_t system_header_start_code;
    bit_buffer.GetBits(32, system_header_start_code);

    uint16_t header_length;
    bit_buffer.GetBits(16, header_length);

    uint8_t marker_bit;
    bit_buffer.GetBits(1, marker_bit);

    uint32_t rate_bound;
    bit_buffer.GetBits(22, rate_bound);

    bit_buffer.GetBits(1, marker_bit);

    uint8_t audio_bound;
    bit_buffer.GetBits(6, audio_bound);

    uint8_t fixed_flag;
    bit_buffer.GetBits(1, fixed_flag);

    uint8_t CSPS_flag;
    bit_buffer.GetBits(1, CSPS_flag);

    uint8_t system_audio_lock_flag;
    bit_buffer.GetBits(1, system_audio_lock_flag);

    uint8_t system_video_lock_flag;
    bit_buffer.GetBits(1, system_video_lock_flag);

    bit_buffer.GetBits(1, marker_bit);

    uint8_t video_bound;
    bit_buffer.GetBits(5, video_bound);

    uint8_t packet_rate_restriction_flag;
    bit_buffer.GetBits(1, packet_rate_restriction_flag);

    uint8_t reserved_bits;
    bit_buffer.GetBits(7, reserved_bits);

    while (true) 
    {
        uint64_t peekbits = 0;
        bit_buffer.PeekBits(1, peekbits);
        if (peekbits != 1)
        {
            break;
        }

        uint8_t stream_id;
        bit_buffer.GetBits(8, stream_id);

        uint8_t ones;
        bit_buffer.GetBits(2, ones);

        uint8_t P_STD_buffer_bound_scale;
        bit_buffer.GetBits(1, P_STD_buffer_bound_scale);

        uint16_t P_STD_buffer_size_bound;
        bit_buffer.GetBits(13, P_STD_buffer_size_bound);
    }

    return 0;
}

int TsReader::CollectAudioPES(BitBuffer& bit_buffer)
{
    audio_pes_.append((const char*)bit_buffer.CurData(), bit_buffer.CurLen());
    return 0;
}

int TsReader::CollectVideoPES(BitBuffer& bit_buffer)
{
    video_pes_.append((const char*)bit_buffer.CurData(), bit_buffer.CurLen());
    return 0;
}

void TsReader::OnVideo(BitBuffer& bit_buffer, const uint32_t& pts, const uint32_t& dts)
{
	const uint8_t* p = bit_buffer.CurData();
    uint32_t len = bit_buffer.BytesLeft();
    size_t i = 0;

    std::cout << "video size=" << bit_buffer.BytesLeft() << ",video frame=\n" 
         << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 256 ? 256 : bit_buffer.BytesLeft()) 
         << std::endl;

    std::vector<std::pair<const uint8_t*, int>> nals;

	if (p[i] == 0x00 && p[i+1] == 0x00 && p[i+2] == 0x00 && p[i+3] == 0x01)
    {   
        const uint8_t* nal = NULL;
        while (i + 3 < len)
        {   
            if (p[i] != 0x00 || p[i+1] != 0x00 || p[i+2] != 0x01)
            {   
                ++i;
                continue;
            }   

            if (nal != NULL)
            {   
                nals.push_back(std::make_pair(nal, p + i - nal - 1));
            }   

            i += 3;
            nal = p + i;
        }   

        if (nal != NULL)
        {   
            nals.push_back(std::make_pair(nal, p + len - nal));
        }   

        std::string header = "";
        for (const auto& kv :nals)
        {
            const uint8_t* nal = kv.first;
            int len = kv.second;
            if (nal != NULL)
            {   
                std::cout << "len=" << len << ", nal=\n" << Util::Bin2Hex(nal, len > 128 ? 128 : len) << std::endl;
                uint8_t nal_ref_idc = nal[0] >> 5;
                uint8_t nal_type = nal[0] & 0x1F;

                std::cout << LMSG << "nal_ref_idc:" << (int)nal_ref_idc << ", nal_type:" << (int) nal_type << std::endl;
                bool dispatch = true;

                uint8_t* nal_ref = (uint8_t*)malloc(len + 4);
                nal_ref[0] = 0x00;
                nal_ref[1] = 0x00;
                nal_ref[2] = 0x00;
                nal_ref[3] = 0x01;
                memcpy(nal_ref + 4, nal, len);
                Payload video_frame(nal_ref, len + 4);
                video_frame.SetVideo();
                video_frame.SetPts(pts / 90);
                video_frame.SetDts(dts / 90);
                if (nal_type == H264NalType_IDR_SLICE)
                {
                    video_frame.SetIFrame();
                }
                else if (nal_type == H264NalType_SLICE)
                {
                    if (pts == dts)
                    {
                        video_frame.SetPFrame();
                    }
                    else
                    {
                        video_frame.SetBFrame();
                    }
                }
                else if (nal_type == H264NalType_SPS)
                {
                    dispatch = false;
                    header.append((const char*)kStartCode, 4);
                    header.append((const char*)nal, len);
                }
                else if (nal_type == H264NalType_PPS)
                {
                    dispatch = false;

                    bool header_complete = ! header.empty();
                    header.append((const char*)kStartCode, 4);
                    header.append((const char*)nal, len);

                    if (header_complete && header_callback_)
                    {
                        uint8_t* nal_ref = (uint8_t*)malloc(header.size());
                        memcpy(nal_ref, header.data(), header.size());
                        Payload header_frame(nal_ref, header.size());
                        header_frame.SetVideo();
                        header_callback_(header_frame);
                    }
                }
                else
                {
                    dispatch = false;
                }

                if (dispatch && frame_callback_)
                {
                    frame_callback_(video_frame);
                }
                else
                {
                    std::cout << LMSG << "no dispatch nal_ref_idc:" << (int)nal_ref_idc << ", nal_type:" << (int) nal_type << std::endl;
                }
            }   
        }
    }
}

void TsReader::OnAudio(BitBuffer& bit_buffer, const uint32_t& pts)
{
    int i = 0;
    double cal_pts = pts;

    while (bit_buffer.BytesLeft() >= 7)
    {
        std::ostringstream os;

        // adts to audio special config
        const uint8_t* p = bit_buffer.CurData();
        uint8_t* audio_header_ref = (uint8_t*)malloc(2);
    	audio_header_ref[0] = 0x00;
    	audio_header_ref[1] = 0x00;
         
        uint32_t syncword = 0;
        bit_buffer.GetBits(12, syncword);
        os << std::hex << "syncword=" << syncword << std::dec;

        uint8_t id = 0;
        bit_buffer.GetBits(1, id);

        uint8_t layer = 0;
        bit_buffer.GetBits(2, layer);

        uint8_t protection_absent = 0;
        bit_buffer.GetBits(1, protection_absent);

        uint8_t profile = 0;
        bit_buffer.GetBits(2, profile);
        profile += 1;

        uint8_t sampling_frequency_index = 0;
        bit_buffer.GetBits(4, sampling_frequency_index);

        int sample_rate = GetSampleRate(sampling_frequency_index);

        uint8_t private_bit = 0;
        bit_buffer.GetBits(1, private_bit);

        uint8_t channel_configuration = 0;
        bit_buffer.GetBits(3, channel_configuration);

        int channles = GetChannles(channel_configuration);

        os << ",sample_rate=" << sample_rate << ",channles=" << channles;

        uint8_t original_copy = 0;
        bit_buffer.GetBits(1, original_copy);

        uint8_t home = 0;
        bit_buffer.GetBits(1, home);

        uint8_t copyright_identification_bit = 0;
        bit_buffer.GetBits(1, copyright_identification_bit);

        uint8_t copyright_identification_start = 0;
        bit_buffer.GetBits(1, copyright_identification_start);

        uint16_t aac_frame_length = 0;
        bit_buffer.GetBits(13, aac_frame_length);

        uint16_t adts_buffer_fullness = 0;
        bit_buffer.GetBits(11, adts_buffer_fullness);

        uint16_t number_of_raw_data_blocks_in_frames = 0;
        bit_buffer.GetBits(2, number_of_raw_data_blocks_in_frames);

        if (! protection_absent)
        {
            uint16_t crc = 0;
            bit_buffer.GetBits(16, crc);
            os << ",crc=" << crc;
        }

        std::cout << LMSG << os.str() << std::endl;

    	audio_header_ref[0] = (profile << 3) | (sampling_frequency_index >> 1); 
    	audio_header_ref[1] = ((sampling_frequency_index & 0x01) << 7) | (channel_configuration << 3); 

        Payload header_frame(audio_header_ref, 2);
        header_frame.SetAudio();
        if (header_callback_)
        {
            header_callback_(header_frame);
        }

        int len = aac_frame_length - ((protection_absent == 1) ? 7 : 9);

        std::cout << LMSG << "aac_frame_length=" << aac_frame_length << ", len=" << len << std::endl;
        if (len > (int)bit_buffer.BytesLeft())
        {
            std::cout << LMSG << "no more than " << len << " bytes left, left " << bit_buffer.BytesLeft() << std::endl;
            break;
        }
        uint8_t* audio_ref = (uint8_t*)malloc(len + 2);
        audio_ref[0] = 0xAF;
        audio_ref[1] = 0x01;
        memcpy(audio_ref + 2, bit_buffer.CurData(), len);
        bit_buffer.SkipBytes(len);

        Payload audio_frame(audio_ref, len + 2);
        audio_frame.SetAudio();
        //audio_frame.SetDts(pts / 90 + i * (sample_rate / 1024.0 / channles));
        audio_frame.SetDts((cal_pts + i * (90000.0/(44100.0/1024.0/2))) / 90.0);
        audio_frame.SetPts(audio_frame.GetDts());
        audio_frame.SetIFrame();
        if (frame_callback_)
        {
            frame_callback_(audio_frame);
        }

        ++i;
    }
}

void TsReader::OpenVideoDumpFile()
{
    if (video_dump_fd_ < 0)
    {
        video_dump_fd_ = open("video.264", O_CREAT|O_TRUNC|O_RDWR, 0664);
    }
}

void TsReader::DumpVideo(const uint8_t* data, const size_t& len)
{
    if (video_dump_fd_ > 0)
    {
        int nbytes = write(video_dump_fd_, data, len);
        UNUSED(nbytes);
    }
}

void TsReader::OpenAudioDumpFile()
{
    if (audio_dump_fd_ < 0)
    {
        audio_dump_fd_ = open("audio.aac", O_CREAT|O_TRUNC|O_RDWR, 0664);
    }
}

void TsReader::DumpAudio(const uint8_t* data, const size_t& len)
{
    if (audio_dump_fd_ > 0)
    {
        int nbytes = write(audio_dump_fd_, data, len);
        UNUSED(nbytes);
    }
}
