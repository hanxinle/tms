#include "io_buffer.h"
#include "socket_util.h"
#include "srt_protocol.h"
#include "srt_socket.h"

using namespace socket_util;

SrtProtocol::SrtProtocol(IoLoop* io_loop, Fd* socket)
    : io_loop_(io_loop)
    , socket_(socket)
    , dump_fd_(-1)
{
    cout << LMSG << "new srt protocol, fd=" << socket->fd() << ", socket=" << (void*)socket_ 
         << ", stream=" << GetSrtSocket()->GetStreamId() << endl;

    ts_reader_.SetFrameCallback(std::bind(&SrtProtocol::OnFrame, this, std::placeholders::_1));
    ts_reader_.SetHeaderCallback(std::bind(&SrtProtocol::OnHeader, this, std::placeholders::_1));
}

SrtProtocol::~SrtProtocol()
{
}

int SrtProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;
    int len = io_buffer.Read(data, io_buffer.Size());

    OpenDumpFile();
    Dump(data, len);

    if (len > 0)
    {
        ts_reader_.ParseTs(data, len);

        return kSuccess;
    }

    return kNoEnoughData;
}

int SrtProtocol::OnStop()
{
    cout << LMSG << "srt protocol stop, fd=" << socket_->fd() << ", socket=" << (void*)socket_ 
         << ", stream=" << GetSrtSocket()->GetStreamId() << endl;

    return kSuccess;
}

int SrtProtocol::OnConnected()
{
    cout << LMSG << "srt protocol connected, fd=" << socket_->fd() << ", socket=" << (void*)socket_ << endl;

	GetSrtSocket()->SetConnected();
    GetSrtSocket()->EnableRead();
    GetSrtSocket()->DisableWrite();

	return 0;
}

int SrtProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    UNUSED(now_in_ms);
    UNUSED(interval);
    UNUSED(count);

    return kSuccess;
}

void SrtProtocol::OpenDumpFile()
{
    if (dump_fd_ == -1)
    {
        ostringstream os;
        os << GetSrtSocket()->GetStreamId() << ".ts";
        dump_fd_ = open(os.str().c_str(), O_CREAT|O_TRUNC|O_RDWR, 0664);
    }
}

void SrtProtocol::Dump(const uint8_t* data, const int& len)
{
    if (dump_fd_ != -1)
    {
        int nbytes = write(dump_fd_, data, len);
        UNUSED(nbytes);
    }
}

void SrtProtocol::OnFrame(const Payload& frame)
{
    if (frame.IsVideo())
    {
        cout << LMSG << (frame.IsIFrame() ? "I" : (frame.IsPFrame() ? "P" : (frame.IsBFrame() ? "B" : "Unknown"))) 
             << ",pts=" << frame.GetPts() << ",dts=" << frame.GetDts() << endl;
    }
    else if (frame.IsAudio())
    {
        cout << LMSG << "audio, dts=" << frame.GetDts() << endl;
    }
}

void SrtProtocol::OnHeader(const Payload& header_frame)
{
    cout << LMSG << "header=" << Util::Bin2Hex(header_frame.GetAllData(), header_frame.GetAllLen()) << endl;

    if (header_frame.IsVideo())
    {
        string video_header((const char*)header_frame.GetAllData(), header_frame.GetAllLen());
    }
    else if (header_frame.IsAudio())
    {
        string audio_header((const char*)header_frame.GetAllData(), header_frame.GetAllLen());
    }
}
