#include "bit_buffer.h"
#include "common_define.h"
#include "ts_reader.h"
#include "util.h"

using namespace std;

const int kTsSegmentFixedSize = 188;
const uint8_t kTsHeaderSyncByte = 0x47;

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
    cout << "ts segment=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;

    uint8_t sync_byte = 0;
    bit_buffer.GetBits(8, sync_byte);

    uint8_t transport_error_indicator = 0;
    bit_buffer.GetBits(1, transport_error_indicator);

    uint8_t payload_unit_start_indicator = 0;
    bit_buffer.GetBits(1, payload_unit_start_indicator);

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

    if (payload_unit_start_indicator)
    {
        uint8_t skip;
        bit_buffer.GetBits(8, skip);
    }

    cout << LMSG << "continuity_counter=" << (int)continuity_counter << endl;

    if (adaptation_field_control == 0x10 || adaptation_field_control == 0x11)
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
    else if (pid == audio_pid_ || pid == video_pid_)
    {
        return ParsePES(bit_buffer);
    }

    return 0;
}

int TsReader::ParseAdaptation(BitBuffer& bit_buffer)
{
    cout << LMSG << "adaptation=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
    cout << LMSG << "bit_buffer left bits=" << bit_buffer.BitsLeft() << ", bytes left=" << bit_buffer.BytesLeft() << endl;

    uint8_t adaptation_field_length = 0;
    bit_buffer.GetBits(8, adaptation_field_length);

    cout << LMSG << "adaptation_field_length=" << (int)adaptation_field_length << endl;

    if (adaptation_field_length == 0)
    {
        return 0;
    }

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
        cout << "before transport_private_data_length=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
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

		//for (i = 0; i < N; i++) 
        //{
		//	reserved 8 bslbf;
		//}
    }

    //for (int i = 0; i < N; i++) 
    {
        uint8_t stuffing_byte;
        bit_buffer.GetBits(8, stuffing_byte);
    }

    return 0;
}

int TsReader::ParsePAT(BitBuffer& bit_buffer)
{
    cout << LMSG << "pat=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
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
    cout << LMSG << "pmt=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;
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
        }
        else if (IsAudioStreamType(stream_type))
        {
            audio_pid_ = elementary_PID;
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

int TsReader::ParsePES(BitBuffer& bit_buffer)
{
    cout << LMSG << "bit_buffer left bits=" << bit_buffer.BitsLeft() << ", bytes left=" << bit_buffer.BytesLeft() << endl;
    cout << LMSG << "pes=\n" << Util::Bin2Hex(bit_buffer.CurData(), bit_buffer.BytesLeft()) << endl;

    return 0;
}
