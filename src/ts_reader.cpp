#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "bit_buffer.h"
#include "common_define.h"
#include "ref_ptr.h"
#include "ts_reader.h"
#include "util.h"

using namespace std;

const int kTsSegmentFixedSize = 188;
const uint8_t kTsHeaderSyncByte = 0x47;

const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

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
        cout << LMSG << "ts len=" << len << ", invalid" << endl;
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

    BitBuffer bit_buffer(data, len);
    //cout << "ts segment=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;

    uint8_t sync_byte = 0;
    bit_buffer.GetBits(8, sync_byte);

    uint8_t transport_error_indicator = 0;
    bit_buffer.GetBits(1, transport_error_indicator);

    uint8_t payload_unit_start_indicator = 0;
    bit_buffer.GetBits(1, payload_unit_start_indicator);

    cout << LMSG << "payload_unit_start_indicator=" << (int)payload_unit_start_indicator << endl;

    uint8_t transport_priority = 0;
    bit_buffer.GetBits(1, transport_priority);

    uint16_t pid = 0xFFFF;
    bit_buffer.GetBits(13, pid);

    cout << LMSG << "pid=" << pid << endl;

    uint8_t transport_scambling_control = 0;
    bit_buffer.GetBits(2, transport_scambling_control);

    uint8_t adaptation_field_control = 0;
    bit_buffer.GetBits(2, adaptation_field_control);

    uint8_t continuity_counter = 0xFF;
    bit_buffer.GetBits(4, continuity_counter);

    if (payload_unit_start_indicator == 0x01 && (pid == 0 || pid == pmt_pid_))
    {
        cout << LMSG << "skip one bytes" << endl;
        uint8_t skip;
        bit_buffer.GetBits(8, skip);
    }

    cout << LMSG << "adaptation_field_control=" << (int)adaptation_field_control << endl;
    cout << LMSG << "continuity_counter=" << (int)continuity_counter << endl;

    if (adaptation_field_control == 2 || adaptation_field_control == 3)
    {
        ParseAdaptation(bit_buffer);
    }

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
    //cout << LMSG << "adt=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
    cout << LMSG << "bit_buffer left bits=" << bit_buffer.BitsLeft() << ", bytes left=" << bit_buffer.BytesLeft() << endl;

    uint8_t adaptation_field_length = 0;
    bit_buffer.GetBits(8, adaptation_field_length);

    cout << LMSG << "adaptation_field_length=" << (int)adaptation_field_length << endl;

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
        cout << LMSG << "PRC" << endl;
		uint64_t program_clock_reference_base = 0;
        bit_buffer.GetBits(33, program_clock_reference_base);
        uint8_t reserved;
        bit_buffer.GetBits(6, reserved);
        uint16_t program_clock_reference_extension;
        bit_buffer.GetBits(9, program_clock_reference_extension);

        cout << "pcr=" << program_clock_reference_base * 300 + program_clock_reference_extension << endl;
    }
    if (OPCR_flag == 0x01)
    {
        cout << LMSG << "OPRC" << endl;
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
        //cout << "before transport_private_data_length=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
		uint8_t transport_private_data_length = 0;
        bit_buffer.GetBits(8, transport_private_data_length);
        cout << "transport_private_data_length=" << (int)transport_private_data_length << endl;
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

        cout << LMSG << "adaptation_field_extension_length=" << (int)adaptation_field_extension_length << endl;
		for (int i = 0; i < adaptation_field_extension_length; i++) 
        {
            bit_buffer.GetBits(8, reserved);
		}
    }

    int end = bit_buffer.BytesLeft();
    adaptation_field_length -= (begin - end);
    cout << LMSG << "adaptation_field_length=" << (int)adaptation_field_length << endl;

    for (int i = 0; i < adaptation_field_length; i++) 
    {
        uint8_t stuffing_byte = 0;
        bit_buffer.GetBits(8, stuffing_byte);
        cout << LMSG << "stuffing_byte=" << std::hex << (int)stuffing_byte << std::dec << endl;
        //assert(stuffing_byte == 0xFF);
    }

    return 0;
}

int TsReader::ParsePAT(BitBuffer& bit_buffer)
{
    //cout << LMSG << "pat=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
    cout << LMSG << "bit_buffer left bits=" << bit_buffer.BitsLeft() << ", bytes left=" << bit_buffer.BytesLeft() << endl;

    uint8_t table_id;
    bit_buffer.GetBits(8, table_id);

    cout << LMSG << "table_id=" << (int)table_id << endl;

    uint8_t section_syntax_indicator;
    bit_buffer.GetBits(1, section_syntax_indicator);

    uint8_t zero;
    bit_buffer.GetBits(1, zero);

    uint8_t reserved;
    bit_buffer.GetBits(2, reserved);

    uint16_t section_length;
    bit_buffer.GetBits(12, section_length);

    cout << LMSG << "section_length=" << section_length << endl;

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

        cout << LMSG << "program_number=" << program_number << endl;

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
            cout << LMSG << "pmt_pid_=" << pmt_pid_ << endl;
        }
    }
    uint32_t CRC_32;
    bit_buffer.GetBits(32, CRC_32);

    return 0;
}

int TsReader::ParsePMT(BitBuffer& bit_buffer)
{
    //cout << LMSG << "pmt=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
    cout << LMSG << "bit_buffer left bits=" << bit_buffer.BitsLeft() << ", bytes left=" << bit_buffer.BytesLeft() << endl;

	uint8_t table_id = 0;
    bit_buffer.GetBits(8, table_id);

    cout << LMSG << "table_id=" << (int)table_id << endl;

	uint8_t section_syntax_indicator = 0;
    bit_buffer.GetBits(1, section_syntax_indicator);

	uint8_t zero = 0;
    bit_buffer.GetBits(1, zero);

	uint8_t reserved = 0;
    bit_buffer.GetBits(2, reserved);

	uint16_t section_length = 0;
    bit_buffer.GetBits(12, section_length);

    cout << LMSG << "section_length=" << section_length << endl;

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
    cout << LMSG << "PCR_PID=" << PCR_PID << endl;

    bit_buffer.GetBits(4, reserved);

	uint16_t program_info_length = 0;
    bit_buffer.GetBits(12, program_info_length);
    cout << LMSG << "program_info_length=" << program_info_length << endl;

    section_length -= 9;
    int es_info_bytes = section_length - 4;

	for (int i = 0; i < program_info_length; i++) 
    {
	    //descriptor();
	}

    cout << LMSG << "es_info_bytes=" << es_info_bytes << endl;

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
            cout << LMSG << "video_pid_=" << video_pid_ << endl;
        }
        else if (IsAudioStreamType(stream_type))
        {
            audio_pid_ = elementary_PID;
            cout << LMSG << "audio_pid_=" << audio_pid_ << endl;
        }

        bit_buffer.GetBits(4, reserved);

	    uint16_t ES_info_length = 0;
        bit_buffer.GetBits(12, ES_info_length);

        cout << LMSG << "stream_type=" << (int)stream_type << ", elementary_PID=" << elementary_PID << ", ES_info_length=" << ES_info_length << endl;

	    for (int i = 0; i < ES_info_length; i++) 
        {
	        //descriptor();
	    }

        int end = bit_buffer.BytesLeft();
        es_info_bytes -= (begin - end);
        cout << LMSG << "es_info_bytes=" << es_info_bytes << endl;
	}

	uint32_t CRC_32 = 0;
    bit_buffer.GetBits(32, CRC_32);
    cout << LMSG << "CRC_32=" << CRC_32 << endl;

    return 0;
}

int TsReader::ParsePES(std::string& pes)
{
    if (pes.empty())
    {
        return 0;
    }

    BitBuffer bit_buffer((const uint8_t*)pes.data(), pes.size());

    cout << LMSG << "bit_buffer left bits=" << bit_buffer.BitsLeft() << ", bytes left=" << bit_buffer.BytesLeft() << endl;
    //cout << LMSG << "pes=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;

	uint32_t packet_start_code_prefix;
    bit_buffer.GetBits(24, packet_start_code_prefix);

	uint8_t stream_id;
    bit_buffer.GetBits(8, stream_id);

    cout << LMSG << "stream_id=" << (int)stream_id << endl;

	uint16_t PES_packet_length;
    bit_buffer.GetBits(16, PES_packet_length);

    cout << LMSG << "PES_packet_length=" << (int)PES_packet_length << endl;

	//if (stream_id != program_stream_map && 
	//	stream_id != padding_stream && 
	//	stream_id != private_stream_2 && 
	//	stream_id != ECM && stream_id != EMM && 
	//	stream_id != program_stream_directory && 
	//	stream_id != DSMCC_stream && 
	//	stream_id != ITU-T Rec. H.222.1 type E stream) 
    if (true)
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

        cout << LMSG << "PTS_DTS_flags=" << (int)PTS_DTS_flags << endl;

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

        cout << LMSG << "PES_header_data_length=" << (int)PES_header_data_length << endl;

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
            cout << LMSG << "dts=pts=" << pts << endl;

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
            cout << LMSG << "pts=" << pts << endl;

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
            cout << LMSG << "dts=" << dts << endl;

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
                string tmp;
                bit_buffer.GetString(16, tmp);
	            //PES_private_data 128 bslbf;
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

        //cout << LMSG << "pes before stuffing_byte=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 128 ? 128 : bit_buffer.BytesLeft()) << endl;

        if (bit_buffer.BytesLeft() > 6 && bit_buffer.CurData()[0] == 0x00 && bit_buffer.CurData()[1] == 0x00 && bit_buffer.CurData()[2] == 0x00 && bit_buffer.CurData()[3] == 0x01)
        {
            cout << LMSG << "is video" << endl;
            if (video_dump_fd_ < 0)
            {
                video_dump_fd_ = open("video.264", O_CREAT|O_TRUNC|O_RDWR, 0664);
            }

            if (video_dump_fd_ > 0)
            {
                write(video_dump_fd_, bit_buffer.CurData(), bit_buffer.BytesLeft());
            }

			OnVideo(bit_buffer, pts, dts);
        }
        else
        {
            cout << LMSG << "is audio" << endl;
            if (audio_dump_fd_ < 0)
            {
                audio_dump_fd_ = open("audio.aac", O_CREAT|O_TRUNC|O_RDWR, 0664);
            }

            if (audio_dump_fd_ > 0)
            {
                write(audio_dump_fd_, bit_buffer.CurData(), bit_buffer.BytesLeft());
            }

            if (frame_callback_)
            {
                if (bit_buffer.BytesLeft() >= 7)
                {
                    // adts to audio special config
                    if (header_callback_)
                    {
                        const uint8_t* p = bit_buffer.CurData();
                        uint8_t* audio_header_ref = (uint8_t*)malloc(2);
    					audio_header_ref[0] = 0x00;
    					audio_header_ref[1] = 0x00;

    					uint8_t audioObjectType = (((uint8_t)p[2]) >> 6) + 1; 
    					uint8_t samplingFrequencyIndex = (((uint8_t)p[2] & 0x3C) >> 2); 
    					uint8_t channelConfiguration  = (((uint8_t)p[2] & 0x01) << 2) | (((uint8_t)p[3] & 0xC0) >> 6); 

    					audio_header_ref[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1); 
    					audio_header_ref[1] = ((samplingFrequencyIndex & 0x01) << 7) | (channelConfiguration << 3); 

                        Payload header_frame(audio_header_ref, 2);
                        header_frame.SetAudio();
                        header_callback_(header_frame);
                    }

                    int len = bit_buffer.BytesLeft() - 7;
                    uint8_t* audio_ref = (uint8_t*)malloc(len);
                    memcpy(audio_ref, bit_buffer.CurData() + 7, len);
                    cout << "audio len=" << len << endl;
                    Payload audio_frame(audio_ref, len);
                    audio_frame.SetAudio();
                    audio_frame.SetDts(dts / 90);
                    audio_frame.SetIFrame();

                    frame_callback_(audio_frame);
                }
            }
        }
	    //for (int i = 0; i < N1; i++) 
        //{
	    //    uint8_t stuffing_byte = 0;
        //    bit_buffer.GetBits(8, stuffing_byte);
	    //}

	    //for (int i = 0; i < N2; i++) 
        //{
	    //    uint8_t PES_packet_data_byte = 0;
        //    bit_buffer.GetBits(8, PES_packet_data_byte);
	    //}
	}
	//else if (stream_id == program_stream_map || 
    //        stream_id == private_stream_2 || 
    //        stream_id == ECM || 
    //        stream_id == EMM || 
    //        stream_id == program_stream_directory || 
    //        stream_id == DSMCC_stream |
    //        stream_id == ITU-T Rec. H.222.1 type E stream ) 
    else if (false)
    {
        cout << LMSG << "pes before stuffing_byte=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 10 ? 10 : bit_buffer.BytesLeft()) << endl;
	    //for (int i = 0; i < PES_packet_length; i++) 
        //{
	    //    uint8_t PES_packet_data_byte = 0;
        //    bit_buffer.GetBits(8, PES_packet_data_byte);
	    //}
	}
	//else if ( stream_id == padding_stream) 
    else if (false)
    {
        cout << LMSG << "pes before stuffing_byte=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 10 ? 10 : bit_buffer.BytesLeft()) << endl;
	    //for (int i = 0; i < PES_packet_length; i++) 
        //{
	    //    uint8_t padding_byte = 0;
        //    bit_buffer.GetBits(8, padding_byte);
	    //}
	}

    pes.clear();

    return 0;
}

int TsReader::PackHead(BitBuffer& bit_buffer)
{
    cout << LMSG << endl;

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

	uint8_t pack_stuffing_length;
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
    cout << LMSG << endl;

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
    cout << LMSG << endl;

    audio_pes_.append((const char*)bit_buffer.CurData(), bit_buffer.CurLen());

    return 0;
}

int TsReader::CollectVideoPES(BitBuffer& bit_buffer)
{
    cout << LMSG << endl;

    video_pes_.append((const char*)bit_buffer.CurData(), bit_buffer.CurLen());

    //cout << "video pes=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft() > 128 ? 128 : bit_buffer.BytesLeft()) << endl;

    return 0;
}

void TsReader::OnVideo(BitBuffer& bit_buffer, const uint32_t& pts, const uint32_t& dts)
{
	const uint8_t* p = bit_buffer.CurData();
    uint32_t len = bit_buffer.BytesLeft();
    size_t i = 0;

    //cout << "nal=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
    vector<pair<const uint8_t*, int>> nals;

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
                nals.push_back(make_pair(nal, p + i - nal));
            }   

            i += 3;
            nal = p + i;
        }   

        if (nal != NULL)
        {   
            nals.push_back(make_pair(nal, p + i - nal));
        }   

        string header = "";
        for (const auto& kv :nals)
        {
            const uint8_t* nal = kv.first;
            int len = kv.second;
            if (nal != NULL)
            {   
                cout << "nal=\n" << Util::Bin2Hex(nal, len > 128 ? 128 : len) << endl;
                uint8_t nal_ref_idc = nal[0] >> 5;
                uint8_t nal_type = nal[0] & 0x1F;

                cout << LMSG << "nal_ref_idc:" << (int)nal_ref_idc << ", nal_type:" << (int) nal_type << endl;

                bool dispatch = true;
                if (frame_callback_)
                {
                    uint8_t* nal_ref = (uint8_t*)malloc(len + 4);
                    nal_ref[0] = 0x00;
                    nal_ref[1] = 0x00;
                    nal_ref[2] = 0x00;
                    nal_ref[3] = 0x01;
                    memcpy(nal_ref + 4, nal, len);
                    Payload video_frame(nal_ref, len);
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

                        if (header_complete)
                        {
                            if (header_callback_)
                            {
                                uint8_t* nal_ref = (uint8_t*)malloc(header.size());
                                memcpy(nal_ref, header.data(), header.size());
                                Payload header_frame(nal_ref, header.size());
                                header_frame.SetVideo();
                                header_callback_(header_frame);
                            }
                        }
                    }
                    else
                    {
                        dispatch = false;
                    }

                    if (dispatch)
                    {
                        frame_callback_(video_frame);
                    }
                }
            }   
        }
    }
}
