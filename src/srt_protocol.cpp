#include "global.h"
#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "socket_util.h"
#include "srt_protocol.h"
#include "srt_socket.h"

using namespace socket_util;

extern LocalStreamCenter g_local_stream_center;

SrtProtocol::SrtProtocol(IoLoop* io_loop, Fd* socket)
    : MediaSubscriber(kSrt)
    , io_loop_(io_loop)
    , socket_(socket)
    , media_publisher_(NULL)
    , dump_fd_(-1)
{
    cout << LMSG << "new srt protocol, fd=" << socket->fd() << ", socket=" << (void*)socket_ << ", stream=" << GetSrtSocket()->GetStreamId() << endl;
	MediaPublisher* media_publisher = g_local_stream_center.GetMediaPublisherByAppStream("srt", GetSrtSocket()->GetStreamId());

    if (media_publisher != NULL)
    {    
        SetMediaPublisher(media_publisher);
        media_publisher->AddSubscriber(this);
        cout << LMSG << "publisher " << media_publisher << " add subscriber for stream " << GetSrtSocket()->GetStreamId() << endl;
    }    
    else
    {
        cout << LMSG << "can't find stream " << GetSrtSocket()->GetStreamId() << ", choose random one to debug"<< endl;
        string app, stream;
        MediaPublisher* media_publisher = g_local_stream_center._DebugGetRandomMediaPublisher(app, stream);
        if (media_publisher)
        {    
            SetMediaPublisher(media_publisher);
            media_publisher->AddSubscriber(this);
            cout << LMSG << "random publisher " << media_publisher << " add subscriber for app " << app << ", stream " << stream << endl;
        }
    }
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
        // FIXME: register one time
        static bool reg = false;

        if (! reg)
        {
            g_local_stream_center.RegisterStream("srt", GetSrtSocket()->GetStreamId(), this);
            cout << LMSG << "register publisher " << this << ", streamid=" << GetSrtSocket()->GetStreamId() << endl;
            reg = true;
        }

        ts_reader_.ParseTs(data, len);

		if (data[0] == 0x47)
        {
            const uint8_t* ts_header = data;
            int l = len;
            while (l >= 188)
            {
                if (ts_header[1] & 0x40)
                {
                    uint8_t adaptation_field_length = ts_header[4];
                    uint16_t pid = ((ts_header[1] & 0x1F) << 8) | ts_header[2];
                    cout << LMSG << "first ts segment, stream id=" << (int)(ts_header[4 + adaptation_field_length + 3]) << ", pid=" << pid << endl;
                    cout << LMSG << "peek 64 bytes\n" << Util::Bin2Hex(ts_header, 64) << endl;
                }
                cout << LMSG << "counter=" << (int)(ts_header[3] & 0x0F) << endl;
                l -= 188;
                ts_header += 188;
            }
        }
        else
        {
            cout << LMSG << "no ts header, len=" << len << endl;
        }

        string media_payload((const char*)data, len);
		for (auto& sub : subscriber_)
        {
            cout << LMSG << "srt route to subscriber " << &sub << endl;
            sub->SendData(media_payload);
        }

        for (auto& pending_sub : wait_header_subscriber_)
        {
            cout << LMSG << "srt route to pending subscriber " << &pending_sub << endl;
            pending_sub->SendData(media_payload);
        }

        static int counter = 0;
        if (counter % 100 == 0)
        {
            //cout << LMSG << "srt payload\n" << Util::Bin2Hex(media_payload) << endl;
        }
        ++counter;

        return kSuccess;
    }

    return kNoEnoughData;
}

int SrtProtocol::OnStop()
{
    cout << LMSG << "srt protocol stop, fd=" << socket_->fd() << ", socket=" << (void*)socket_ << ", stream=" << GetSrtSocket()->GetStreamId() << endl;
    if (media_publisher_)
    {
        media_publisher_->RemoveSubscriber(this);
    }

    return kSuccess;
}

int SrtProtocol::OnConnected()
{
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

int SrtProtocol::SendData(const std::string& data)
{
    return GetSrtSocket()->Send((const uint8_t*)data.data(), data.size());
}

void SrtProtocol::OpenDumpFile()
{
    if (dump_fd_ == -1)
    {
        ostringstream os;
        os << this << ".ts";
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
