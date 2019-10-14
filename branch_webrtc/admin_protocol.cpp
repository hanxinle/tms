#include "admin_protocol.h"
#include "global.h"
#include "io_buffer.h"
#include "rtmp_protocol.h"
#include "socket_util.h"
#include "tcp_socket.h"

using namespace socket_util;

AdminProtocol::AdminProtocol(Epoller* epoller, Fd* socket)
    :
    epoller_(epoller),
    socket_(socket)
{
}

AdminProtocol::~AdminProtocol()
{
}

int AdminProtocol::Parse(IoBuffer& io_buffer)
{
    uint8_t* data = NULL;
    size_t len = 0;
    len = io_buffer.Peek(data, 0, io_buffer.Size());

    cout << LMSG << endl;

    if (len > 2)
    {
        if (len > 4096)
        {
            cout << LMSG << "too big admin msg" << endl;
            return kClose;
        }

        string admin_msg((const char*)data, len - 2);
        cout << LMSG << " recv admin msg[" << admin_msg << "]" << endl;

        if (data[len - 2] == '\r' && data[len - 1] == '\n')
        {
            io_buffer.Skip(len);

            ProcAdminMsg(admin_msg);

            return kSuccess;
        }
    }

    return kNoEnoughData;
}

int AdminProtocol::OnStop()
{
    return kSuccess;
}

int AdminProtocol::OnConnected()
{
	GetTcpSocket()->SetConnected();
    GetTcpSocket()->EnableRead();
    GetTcpSocket()->DisableWrite();

	return 0;
}

int AdminProtocol::EveryNSecond(const uint64_t& now_in_ms, const uint32_t& interval, const uint64_t& count)
{
    UNUSED(now_in_ms);
    UNUSED(interval);
    UNUSED(count);

    return kSuccess;
}

int AdminProtocol::ProcAdminMsg(const string& admin_msg)
{
    cout << LMSG << endl;
    vector<string> vec_msg = Util::SepStr(admin_msg, " ");

    for (const auto& v : vec_msg)
    {
        cout << LMSG << "'" << v << "'" << endl;
    }

    if (vec_msg.empty())
    {
        return kError;
    }

    if (vec_msg[0] == "rtmp")
    {
        if (vec_msg.size() == 4)
        {
            if (vec_msg[1] == "pull")
            {
                if (vec_msg[2] == "url")
                {
                    string rtmp_url = vec_msg[3];

                    cout << LMSG << "pull rtmp_url:'" << rtmp_url << "'" << endl;

                    RtmpUrl rtmp_info;

                    int ret = RtmpProtocol::ParseRtmpUrl(rtmp_url, rtmp_info);

                    if (ret != 0)
                    {
                        cout << LMSG << endl;
                        return kError;
                    }

                    
                    int fd = CreateNonBlockTcpSocket();
                    if (fd < 0)
                    {
                        cout << LMSG << endl;
                    }

				    ret = ConnectHost(fd, rtmp_info.ip, rtmp_info.port);

    			    if (ret < 0 && errno != EINPROGRESS)
    			    {    
    			        cout << LMSG << "Connect ret:" << ret << endl;
    			        return -1;
    			    }

                    Fd* socket = new TcpSocket(epoller_, fd, (SocketHandle*)g_rtmp_mgr);

				    RtmpProtocol* rtmp_player = g_rtmp_mgr->GetOrCreateProtocol(*socket);

    			    rtmp_player->SetApp(rtmp_info.app);
    			    rtmp_player->SetStreamName(rtmp_info.stream);
                    rtmp_player->SetDomain(rtmp_info.ip);
                    rtmp_player->SetArgs(rtmp_info.args);
    			    rtmp_player->SetPullServer();

    			    if (errno == EINPROGRESS)
    			    {    
    			        rtmp_player->GetTcpSocket()->SetConnecting();
    			        rtmp_player->GetTcpSocket()->EnableWrite();
    			    }    
    			    else 
    			    {    
    			        rtmp_player->GetTcpSocket()->SetConnected();
    			        rtmp_player->GetTcpSocket()->EnableRead();

    			        rtmp_player->SendHandShakeStatus0();
    			        rtmp_player->SendHandShakeStatus1();
    			    }    

    			    cout << LMSG << endl;
                }
                else if (vec_msg[2] == "detail")
                {
                    string detail = vec_msg[3];

                    vector<string> vec_msg = Util::SepStr(detail, ",");
                    map<string, string> info;

                    for (const auto& v : vec_msg)
                    {
                        vector<string> tmp = Util::SepStr(v, "=");

                        if (tmp.size() == 2)
                        {
                            info[tmp[0]] = tmp[1];
                        }
                    }

                    if (info.count("ip") == 0 || info.count("port") == 0 || info.count("domain") == 0 || info.count("app") == 0 || info.count("stream") == 0)
                    {
                        cout << LMSG << "detail format error" << endl;
                        return kClose;
                    }

                    int fd = CreateNonBlockTcpSocket();
                    if (fd < 0)
                    {
                        cout << LMSG << endl;
                    }

                    RtmpUrl rtmp_info;

                    rtmp_info.ip = info["domain"];
                    rtmp_info.port = Util::Str2Num<uint16_t>(info["port"]);
                    rtmp_info.app = info["app"];
                    rtmp_info.stream = info["stream"];

				    int ret = ConnectHost(fd, info["ip"], rtmp_info.port);

    			    if (ret < 0 && errno != EINPROGRESS)
    			    {    
    			        cout << LMSG << "Connect ret:" << ret << endl;
    			        return -1;
    			    }

                    Fd* socket = new TcpSocket(epoller_, fd, (SocketHandle*)g_rtmp_mgr);

				    RtmpProtocol* rtmp_player = g_rtmp_mgr->GetOrCreateProtocol(*socket);

    			    rtmp_player->SetApp(rtmp_info.app);
    			    rtmp_player->SetStreamName(rtmp_info.stream);
                    rtmp_player->SetArgs(rtmp_info.args);
                    rtmp_player->SetDomain(rtmp_info.ip);
    			    rtmp_player->SetPullServer();

    			    if (errno == EINPROGRESS)
    			    {    
    			        rtmp_player->GetTcpSocket()->SetConnecting();
    			        rtmp_player->GetTcpSocket()->EnableWrite();
    			    }    
    			    else 
    			    {    
    			        rtmp_player->GetTcpSocket()->SetConnected();
    			        rtmp_player->GetTcpSocket()->EnableRead();

    			        rtmp_player->SendHandShakeStatus0();
    			        rtmp_player->SendHandShakeStatus1();
    			    }    

    			    cout << LMSG << endl;
                }

                return kSuccess;
            }
            else
            {
            }
        }
    }

    return kError;
}
