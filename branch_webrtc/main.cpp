#include <signal.h>

#include <iostream>

#include "admin_mgr.h"
#include "any.h"
#include "base_64.h"
#include "bit_buffer.h"
#include "bit_stream.h"
#include "echo_mgr.h"
#include "epoller.h"
#include "http_file_mgr.h"
#include "http_flv_mgr.h"
#include "http_hls_mgr.h"
#include "local_stream_center.h"
#include "media_center_mgr.h"
#include "media_node_discovery_mgr.h"
#include "ref_ptr.h"
#include "rtmp_mgr.h"
#include "server_mgr.h"
#include "socket_util.h"
#include "srt/srt.h"
#include "ssl_socket.h"
#include "tcp_socket.h"
#include "timer_in_second.h"
#include "timer_in_millsecond.h"
#include "trace_tool.h"
#include "udp_socket.h"
#include "util.h"
#include "webrtc_mgr.h"
#include "web_socket_mgr.h"

#include "openssl/ssl.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

// debug
#include "webrtc_protocol.h"

using namespace any;
using namespace std;
using namespace socket_util;

static void sighandler(int sig_no)
{
    cout << LMSG << "sig:" << sig_no << endl;
	exit(0);
} 

LocalStreamCenter       g_local_stream_center;
NodeInfo                g_node_info;
Epoller*                g_epoll = NULL;
HttpFlvMgr*             g_http_flv_mgr = NULL;
HttpFlvMgr*             g_https_flv_mgr = NULL;
HttpHlsMgr*             g_http_hls_mgr = NULL;
HttpHlsMgr*             g_https_hls_mgr = NULL;
MediaCenterMgr*         g_media_center_mgr = NULL;
MediaNodeDiscoveryMgr*  g_media_node_discovery_mgr = NULL;
RtmpMgr*                g_rtmp_mgr = NULL;
ServerMgr*              g_server_mgr = NULL;
WebrtcMgr*              g_webrtc_mgr = NULL;
SSL_CTX*                g_tls_ctx = NULL;
SSL_CTX*                g_dtls_ctx = NULL;
string                  g_dtls_fingerprint = "";
string                  g_local_ice_pwd = "";
string                  g_local_ice_ufrag = "";
string                  g_remote_ice_pwd = "";
string                  g_remote_ice_ufrag = "";
string                  g_server_ip = "";
WebrtcProtocol*         g_debug_webrtc = NULL;
int                     g_srt_client_fd = -1;

void AvLogCallback(void* ptr, int level, const char* fmt, va_list vl)
{
    UNUSED(ptr);
    UNUSED(level);

    vprintf(fmt, vl);
}

int main(int argc, char* argv[])
{
    // srt
    srt_startup();
    //srt_setloglevel(logging::LogLevel::debug);

	addrinfo hints;
    addrinfo* res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    string service("9000");

    if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res))
    {
        cout << "illegal port number or port is busy" << endl;
        return 0;
    }

    SRTSOCKET serv = srt_socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    SRT_TRANSTYPE tt = SRTT_LIVE;
    srt_setsockopt(serv, 0, SRTO_TRANSTYPE, &tt, sizeof tt);

    bool sync = false;

    srt_setsockopt(serv, 0, SRTO_RCVSYN, &sync, sizeof sync);

    if (SRT_ERROR == srt_bind(serv, res->ai_addr, res->ai_addrlen))
    {
        cout << "bind: " << srt_getlasterror_str() << endl;
        return 0;
    }

    freeaddrinfo(res);

	srt_listen(serv, 10);
    // srt end

    av_register_all();
    avcodec_register_all();

    av_log_set_callback(AvLogCallback);
    av_log_set_level(AV_LOG_VERBOSE);

    CRC32 crc32(CRC32_STUN);

    uint8_t crc[] = {1, 2, 3, 4, 5, 6};
    cout << LMSG << crc32.GetCrc32(crc, 6) << endl;

    {
        static uint8_t kSps[] = {0x67, 0x64, 0x00, 0x33, 0xAC,0x2C,0xAC,0x07,0x80,0x22,0x7E,0x5C,0x05,0xA8,0x08,0x08,0x0A,0x00,0x00,0x07,0xD0,0x00,0x01,0xD4,0xC1,0x08};
        static uint8_t kPps[] = {0x68, 0xEE, 0x3C, 0xB0};

        string tmp((const char*)kSps, sizeof(kSps));

        string t;
        Base64::Encode(tmp, t);
        cout << t << endl;
    }

    string raw = "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure.";

    string base64;

    Base64::Encode(raw, base64);

    cout << base64 << endl;

    string tmp;
    Base64::Decode(base64, tmp);

    cout << LMSG <<raw << endl;
    cout << LMSG <<tmp << endl;

    cout << LMSG << raw.size() << endl;
    cout << LMSG << base64.size() << endl;
    cout << LMSG << tmp.size() << endl;

    cout << Util::Bin2Hex(raw, 32, false, "prefix:") << endl;
    cout << LMSG << endl;

    assert(raw == tmp);

	string server_crt = Util::ReadFile("server.crt");
	string server_key = Util::ReadFile("server.key");

    if (server_crt.empty() || server_key.empty())
    {
        cout << LMSG << "server.crt or server.key incorrect" << endl;
        return -1;
    }

    // Open ssl init
	SSL_load_error_strings();
    int ret = SSL_library_init();

    assert(ret == 1);

    // tls init
    g_tls_ctx = SSL_CTX_new(SSLv23_method());

    assert(g_tls_ctx != NULL);

	BIO *tls_mem_cert = BIO_new_mem_buf((void *)server_crt.c_str(), server_crt.length());
    X509 *tls_cert= PEM_read_bio_X509(tls_mem_cert,NULL,NULL,NULL);
    SSL_CTX_use_certificate(g_tls_ctx, tls_cert);
    X509_free(tls_cert);
    BIO_free(tls_mem_cert);    
    
    BIO *tls_mem_key = BIO_new_mem_buf((void *)server_key.c_str(), server_key.length());
    RSA *tls_rsa_private = PEM_read_bio_RSAPrivateKey(tls_mem_key, NULL, NULL, NULL);
    SSL_CTX_use_RSAPrivateKey(g_tls_ctx, tls_rsa_private);
    RSA_free(tls_rsa_private);
    BIO_free(tls_mem_key);

    ret = SSL_CTX_check_private_key(g_tls_ctx);

    // dtls init
    //g_dtls_ctx = SSL_CTX_new(DTLSv1_2_method());

#if 0 
    // 用导入证书的方法,目前不可行
    g_dtls_ctx = SSL_CTX_new(DTLSv1_method());
	BIO *dtls_mem_cert = BIO_new_mem_buf((void *)server_crt.c_str(), server_crt.length());
    X509 *dtls_cert= PEM_read_bio_X509(dtls_mem_cert,NULL,NULL,NULL);
    SSL_CTX_use_certificate(g_dtls_ctx, dtls_cert);
    X509_free(dtls_cert);
    BIO_free(dtls_mem_cert);    
    
    BIO *dtls_mem_key = BIO_new_mem_buf((void *)server_key.c_str(), server_key.length());
    RSA *dtls_rsa_private = PEM_read_bio_RSAPrivateKey(dtls_mem_key, NULL, NULL, NULL);
    SSL_CTX_use_RSAPrivateKey(g_dtls_ctx, dtls_rsa_private);
    RSA_free(dtls_rsa_private);
    BIO_free(dtls_mem_key);

    ret = SSL_CTX_check_private_key(g_dtls_ctx);
#else
    // 代码里生成证书
	EVP_PKEY* dtls_private_key = EVP_PKEY_new();
    if (dtls_private_key == NULL)
    {
        cout << LMSG << "EVP_PKEY_new err" << endl;
        return -1;
    }

    RSA* rsa = RSA_new();
    if (rsa == NULL)
    {
        cout << LMSG << "RSA_new err" << endl;
        return -1;
    }

    BIGNUM* exponent = BN_new();
    if (exponent == NULL)
    {
        cout << LMSG << "BN_new err" << endl;
        return -1;
    }

    BN_set_word(exponent, RSA_F4);

	const string& aor = "www.john.com";
	int expire_day = 365;
	int private_key_len = 1024;

    RSA_generate_key_ex(rsa, private_key_len, exponent, NULL);

    ret = EVP_PKEY_set1_RSA(dtls_private_key, rsa);
    if (ret != 1)
    {
        cout << LMSG << "EVP_PKEY_set1_RSA err:" << ret << endl;
    }

    X509* dtls_cert = X509_new();
    if (dtls_cert == NULL)
    {
        cout << LMSG << "X509_new err" << endl;
        return -1;
    }

    X509_NAME* subject = X509_NAME_new();
    if (subject == NULL)
    {
        cout << LMSG << "X509_NAME_new err" << endl;
        return -1;
    }

    int serial = rand();
    ASN1_INTEGER_set(X509_get_serialNumber(dtls_cert), serial);

    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, (unsigned char *) aor.data(), aor.size(), -1, 0);

    X509_set_issuer_name(dtls_cert, subject);
    X509_set_subject_name(dtls_cert, subject);

    const long cert_duration = 60*60*24*expire_day;

    X509_gmtime_adj(X509_get_notBefore(dtls_cert), 0);
    X509_gmtime_adj(X509_get_notAfter(dtls_cert), cert_duration);

    ret = X509_set_pubkey(dtls_cert, dtls_private_key);
    if (ret != 1)
    {
        cout << LMSG << "X509_set_pubkey err:" << ret << endl;
    }

    ret = X509_sign(dtls_cert, dtls_private_key, EVP_sha1());
    if (ret == 0)
    {
        cout << LMSG << "X509_sign err:" << ret << endl;
    }

    // free
    RSA_free(rsa);
    BN_free(exponent);
    X509_NAME_free(subject);

    g_dtls_ctx = SSL_CTX_new(DTLSv1_2_method());
	ret = SSL_CTX_use_certificate(g_dtls_ctx, dtls_cert);
    if (ret != 1)
    {   
        cout << LMSG << "|SSL_CTX_use_certificate error:" << ret << endl; 
    }   

    ret = SSL_CTX_use_PrivateKey(g_dtls_ctx, dtls_private_key);
    if (ret != 1)
    {   
        cout << LMSG << "|SSL_CTX_use_PrivateKey error:" << ret << endl; 
    }   

    ret = SSL_CTX_set_cipher_list(g_dtls_ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    if (ret != 1)
    {   
        cout << LMSG << "|SSL_CTX_set_cipher_list error:" << ret << endl; 
    }   

    ret = SSL_CTX_set_tlsext_use_srtp(g_dtls_ctx, "SRTP_AES128_CM_SHA1_80");
    if (ret != 0)
    {   
        cout << LMSG << "|SSL_CTX_set_tlsext_use_srtp error:" << ret << endl; 
    }   

    SSL_CTX_set_verify_depth (g_dtls_ctx, 4); 
    SSL_CTX_set_read_ahead(g_dtls_ctx, 1);
#endif

    // dtls fingerprint
    char fp[100];
    char *fingerprint = fp;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int i; 
    unsigned int n; 
 
    int r = X509_digest(dtls_cert, EVP_sha256(), md, &n);
    
    for (i = 0; i < n; i++)
    {
        sprintf(fingerprint, "%02X", md[i]);
        fingerprint += 2;
        
        if(i < (n-1))
        {
            *fingerprint = ':';
        }
        else
        {
            *fingerprint = '\0';
        }
        ++fingerprint;
    }

    g_dtls_fingerprint.assign(fp, strlen(fp));

    cout << "DTLS fingerprint[" << g_dtls_fingerprint << "]" << endl;

    // parse args
    map<string, string> args_map = Util::ParseArgs(argc, argv);

    uint16_t rtmp_port              = 1935;
    uint16_t https_file_port        = 8643;
    uint16_t http_file_port         = 8666;
    uint16_t https_flv_port         = 8743;
    uint16_t http_flv_port          = 8787;
    uint16_t https_hls_port         = 8843;
    uint16_t http_hls_port          = 8888;
    uint16_t server_port            = 10001;
    uint16_t admin_port             = 11000;
    uint16_t web_socket_port        = 8901;
    uint16_t ssl_web_socket_port    = 8943;
    uint16_t echo_port              = 11345;
    uint16_t webrtc_port            = 11445;
    bool daemon                     = false;

    auto iter_server_ip     = args_map.find("server_ip");
    auto iter_rtmp_port     = args_map.find("rtmp_port");
    auto iter_http_flv_port = args_map.find("http_flv_port");
    auto iter_http_hls_port = args_map.find("http_hls_port");
    auto iter_server_port   = args_map.find("server_port");
    auto iter_admin_port    = args_map.find("admin_port");
    auto iter_daemon        = args_map.find("daemon");

    if (iter_server_ip == args_map.end())
    {
        cout << "Usage:" << argv[0] << " -server_ip <xxx.xxx.xxx.xxx> -server_port [xxx] -http_flv_port [xxx] -http_hls_port [xxx] -daemon [xxx]" << endl;
        return 0;
    }

    g_server_ip = iter_server_ip->second;

    if (iter_rtmp_port != args_map.end())
    {
        if (! iter_rtmp_port->second.empty())
        {
            rtmp_port = Util::Str2Num<uint16_t>(iter_rtmp_port->second);
        }
    }

    if (iter_http_flv_port != args_map.end())
    {
        if (! iter_http_flv_port->second.empty())
        {
            http_flv_port = Util::Str2Num<uint16_t>(iter_http_flv_port->second);
        }
    }

    if (iter_http_hls_port != args_map.end())
    {
        if (! iter_http_hls_port->second.empty())
        {
            http_hls_port = Util::Str2Num<uint16_t>(iter_http_hls_port->second);
        }
    }

    if (iter_server_port != args_map.end())
    {
        if (! iter_server_port->second.empty())
        {
            server_port = Util::Str2Num<uint16_t>(iter_server_port->second);
        }
    }

    if (iter_admin_port != args_map.end())
    {
        if (! iter_admin_port->second.empty())
        {
            admin_port = Util::Str2Num<uint16_t>(iter_admin_port->second);
        }
    }

    if (iter_daemon != args_map.end())
    {
        int tmp = Util::Str2Num<int>(iter_daemon->second);

        daemon = (! (tmp == 0));
    }

    if (daemon)
    {
        Util::Daemon();
    }

	IpStr2Num(g_server_ip, g_node_info.ip);
    g_node_info.port.push_back(server_port);
    g_node_info.type          = RTMP_NODE;
    g_node_info.start_time_ms = Util::GetNowMs();
    g_node_info.pid           = getpid();

	signal(SIGUSR1, sighandler);
    signal(SIGPIPE,SIG_IGN);

    Log::SetLogLevel(kLevelDebug);

    DEBUG << argv[0] << " starting..." << endl;

    Epoller epoller;
    g_epoll = &epoller;

    // === Init Timer ===
    TimerInSecond timer_in_second(&epoller);
    TimerInMillSecond timer_in_millsecond(&epoller);

    // === Init Server Stream Socket ===
    int server_stream_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_stream_fd);
    Bind(server_stream_fd, "0.0.0.0", server_port);
    Listen(server_stream_fd);
    SetNonBlock(server_stream_fd);

    ServerMgr server_mgr(&epoller);
    timer_in_second.AddTimerSecondHandle(&server_mgr);
    g_server_mgr = &server_mgr;

    TcpSocket server_stream_socket(&epoller, server_stream_fd, &server_mgr);
    server_stream_socket.EnableRead();
    server_stream_socket.AsServerSocket();

    // === Init Server Rtmp Socket ===
    int server_rtmp_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_rtmp_fd);
    Bind(server_rtmp_fd, "0.0.0.0", rtmp_port);
    Listen(server_rtmp_fd);
    SetNonBlock(server_rtmp_fd);

    RtmpMgr rtmp_mgr(&epoller, &server_mgr);
    timer_in_second.AddTimerSecondHandle(&rtmp_mgr);
    g_rtmp_mgr = &rtmp_mgr;

    TcpSocket server_rtmp_socket(&epoller, server_rtmp_fd, &rtmp_mgr);
    server_rtmp_socket.EnableRead();
    server_rtmp_socket.AsServerSocket();

    // === Init Server Http Flv Socket ===
    int server_http_flv_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_http_flv_fd);
    Bind(server_http_flv_fd, "0.0.0.0", http_flv_port);
    Listen(server_http_flv_fd);
    SetNonBlock(server_http_flv_fd);

    HttpFlvMgr http_flv_mgr(&epoller);

    g_http_flv_mgr = &http_flv_mgr;

    TcpSocket server_http_flv_socket(&epoller, server_http_flv_fd, &http_flv_mgr);
    server_http_flv_socket.EnableRead();
    server_http_flv_socket.AsServerSocket();

    // === Init Server Https Flv Socket ===
    int server_https_flv_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_https_flv_fd);
    Bind(server_https_flv_fd, "0.0.0.0", https_flv_port);
    Listen(server_https_flv_fd);
    SetNonBlock(server_https_flv_fd);

    HttpFlvMgr https_flv_mgr(&epoller);

    g_https_flv_mgr = &https_flv_mgr;

    SslSocket server_https_flv_socket(&epoller, server_https_flv_fd, &https_flv_mgr);
    server_https_flv_socket.EnableRead();
    server_https_flv_socket.AsServerSocket();

    // === Init Server Http Hls Socket ===
    int server_http_hls_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_http_hls_fd);
    Bind(server_http_hls_fd, "0.0.0.0", http_hls_port);
    Listen(server_http_hls_fd);
    SetNonBlock(server_http_hls_fd);

    HttpHlsMgr http_hls_mgr(&epoller, &rtmp_mgr, &server_mgr);

    g_http_hls_mgr = &http_hls_mgr;

    TcpSocket server_http_hls_socket(&epoller, server_http_hls_fd, &http_hls_mgr);
    server_http_hls_socket.EnableRead();
    server_http_hls_socket.AsServerSocket();

    // === Init Server Https Hls Socket ===
    int server_https_hls_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_https_hls_fd);
    Bind(server_https_hls_fd, "0.0.0.0", https_hls_port);
    Listen(server_https_hls_fd);
    SetNonBlock(server_https_hls_fd);

    HttpHlsMgr https_hls_mgr(&epoller, &rtmp_mgr, &server_mgr);

    g_https_hls_mgr = &https_hls_mgr;

    SslSocket server_https_hls_socket(&epoller, server_https_hls_fd, &https_hls_mgr);
    server_https_hls_socket.EnableRead();
    server_https_hls_socket.AsServerSocket();

    // === Init Admin Socket ===
    int admin_fd = CreateNonBlockTcpSocket();

    ReuseAddr(admin_fd);
    Bind(admin_fd, "0.0.0.0", admin_port);
    Listen(admin_fd);
    SetNonBlock(admin_fd);

    AdminMgr admin_mgr(&epoller);

    TcpSocket admin_socket(&epoller, admin_fd, &admin_mgr);
    admin_socket.EnableRead();
    admin_socket.AsServerSocket();

    // === Init WebSocket Socket ===
    int web_socket_fd = CreateNonBlockTcpSocket();

    ReuseAddr(web_socket_fd);
    Bind(web_socket_fd, "0.0.0.0", web_socket_port);
    Listen(web_socket_fd);
    SetNonBlock(web_socket_fd);

    WebSocketMgr web_socket_mgr(&epoller);

    TcpSocket web_socket_socket(&epoller, web_socket_fd, &web_socket_mgr);
    web_socket_socket.EnableRead();
    web_socket_socket.AsServerSocket();

    // === Init SSL WebSocket Socket ===
    int ssl_web_socket_fd = CreateNonBlockTcpSocket();

    ReuseAddr(ssl_web_socket_fd);
    Bind(ssl_web_socket_fd, "0.0.0.0", ssl_web_socket_port);
    Listen(ssl_web_socket_fd);
    SetNonBlock(ssl_web_socket_fd);

    WebSocketMgr ssl_web_socket_mgr(&epoller);

    SslSocket ssl_web_socket_socket(&epoller, ssl_web_socket_fd, &ssl_web_socket_mgr);
    ssl_web_socket_socket.EnableRead();
    ssl_web_socket_socket.AsServerSocket();

    // === Init Server Http File Socket ===
    int server_https_file_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_https_file_fd);
    Bind(server_https_file_fd, "0.0.0.0", https_file_port);
    Listen(server_https_file_fd);
    SetNonBlock(server_https_file_fd);

    HttpFileMgr https_file_mgr(&epoller);

    SslSocket server_https_file_socket(&epoller, server_https_file_fd, &https_file_mgr);
    server_https_file_socket.EnableRead();
    server_https_file_socket.AsServerSocket();

    // === Init Server Http File Socket ===
    int server_http_file_fd = CreateNonBlockTcpSocket();

    ReuseAddr(server_http_file_fd);
    Bind(server_http_file_fd, "0.0.0.0", http_file_port);
    Listen(server_http_file_fd);
    SetNonBlock(server_http_file_fd);

    HttpFileMgr http_file_mgr(&epoller);

    TcpSocket server_http_file_socket(&epoller, server_http_file_fd, &http_file_mgr);
    server_http_file_socket.EnableRead();
    server_http_file_socket.AsServerSocket();

    // === Init Udp Echo Socket ===
    int echo_fd = CreateNonBlockUdpSocket();

    ReuseAddr(echo_fd);
    Bind(echo_fd, "0.0.0.0", echo_port);
    SetNonBlock(echo_fd);

    EchoMgr echo_mgr(&epoller);

    UdpSocket server_echo_socket(&epoller, echo_fd, &echo_mgr);
    server_echo_socket.EnableRead();

    // === Init Udp Echo Socket ===
    int webrtc_fd = CreateNonBlockUdpSocket();

    ReuseAddr(webrtc_fd);
    Bind(webrtc_fd, "0.0.0.0", webrtc_port);
    SetNonBlock(webrtc_fd);

    WebrtcMgr webrtc_mgr(&epoller);

    g_webrtc_mgr = &webrtc_mgr;

    timer_in_second.AddTimerSecondHandle(&webrtc_mgr);
    timer_in_millsecond.AddTimerMillSecondHandle(&webrtc_mgr);

    UdpSocket server_webrtc_socket(&epoller, webrtc_fd, &webrtc_mgr);
    server_webrtc_socket.EnableRead();

    // === Init Media Center ===
    MediaCenterMgr media_center_mgr(&epoller);
    timer_in_second.AddTimerSecondHandle(&media_center_mgr);
    g_media_center_mgr = &media_center_mgr;

    // === Init Media Node Discovery ===
    MediaNodeDiscoveryMgr media_node_discovery_mgr(&epoller);
    g_media_node_discovery_mgr = &media_node_discovery_mgr;
    media_node_discovery_mgr.ConnectNodeDiscovery("127.0.0.1", 16001);

    timer_in_second.AddTimerSecondHandle(&media_node_discovery_mgr);

    sockaddr_storage clientaddr;
    int addrlen = sizeof(clientaddr);
	SRTSOCKET fhandle;

    // Event Loop
    while (true)
    {
        epoller.Run();

		if (SRT_INVALID_SOCK == (fhandle = srt_accept(serv, (sockaddr*)&clientaddr, &addrlen)))
        {
            int err = srt_getlasterror(NULL);


            if (err == (MJ_AGAIN * 1000 + MN_RDAVAIL))
            {
                continue;
            }

            cout << "err:" << err << endl;

            cout << "accept: " << srt_getlasterror_str() << endl;
            return 0;
        }

        cout << "accept success" << endl;

        int tsbpdmode = 1;
        srt_setsockopt(fhandle, 0, SRTO_TSBPDMODE, &tsbpdmode, sizeof tsbpdmode);

        int tsbpddelay = 1000;
        srt_setsockopt(fhandle, 0, SRTO_TSBPDDELAY, &tsbpddelay, sizeof tsbpddelay);

        bool sync = false;
        srt_setsockopt(fhandle, 0, SRTO_SNDSYN, &sync, sizeof sync);

        g_srt_client_fd = fhandle;
    }

    return 0;
}
