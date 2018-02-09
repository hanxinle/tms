#ifndef __RTP_HEADER_H__
#define __RTP_HEADER_H__

#include <netinet/in.h>

#define RTCP_Sender_PT       200 // RTCP Sender Report
#define RTCP_Receiver_PT     201 // RTCP Receiver Report
#define RTCP_SDES_PT         202
#define RTCP_BYE             203
#define RTCP_APP             204
#define RTCP_RTP_Feedback_PT 205 // RTCP Transport Layer Feedback Packet
#define RTCP_PS_Feedback_PT  206 // RTCP Payload Specific Feedback Packet

#define RTCP_PLI_FMT           1
#define RTCP_SLI_FMT           2
#define RTCP_FIR_FMT           4
#define RTCP_AFB              15

#define VP8_90000_PT        100 // VP8 Video Codec
#define VP9_90000_PT        101 // VP9 Video Codec
#define OPUS_48000_PT       111 // Opus Audio Codec

class RtpHeader
{
public:
    static const int MIN_SIZE = 12;
    uint32_t cc :4;
    uint32_t hasextension :1;
    uint32_t padding :1;
    uint32_t version :2;
    uint32_t payloadtype :7;
    uint32_t marker :1;
    uint32_t seqnum :16;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t extensionpayload:16;
    uint32_t extensionlength:16;

    uint32_t extensions;

    inline RtpHeader()
        :
        cc(0),
        hasextension(0),
        padding(0),
        version(2),
        payloadtype(0),
        marker(0),
        seqnum(0),
        timestamp(0),
        ssrc(0),
        extensionpayload(0),
        extensionlength(0)
    {
    }

    inline uint8_t hasPadding() const
    {
        return padding;
    }

    inline uint8_t getVersion() const
    {
        return version;
    }
    inline void setVersion(uint8_t aVersion)
    {
        version = aVersion;
    }
    inline uint8_t getMarker() const
    {
        return marker;
    }
    inline void setMarker(uint8_t aMarker)
    {
        marker = aMarker;
    }
    inline uint8_t getExtension() const
    {
        return hasextension;
    }
    inline void setExtension(uint8_t ext)
    {
        hasextension = ext;
    }
    inline uint8_t getCc() const
    {
        return cc;
    }
    inline void setCc (uint8_t theCc)
    {
        cc = theCc;
    }
    inline uint8_t getPayloadType() const
    {
        return payloadtype;
    }
    inline void setPayloadType(uint8_t aType)
    {
        payloadtype = aType;
    }
    inline uint16_t getSeqNumber() const
    {
        return ntohs(seqnum);
    }
    inline void setSeqNumber(uint16_t aSeqNumber)
    {
        seqnum = htons(aSeqNumber);
    }
    inline uint32_t getTimestamp() const
    {
        return ntohl(timestamp);
    }
    inline void setTimestamp(uint32_t aTimestamp)
    {
        timestamp = htonl(aTimestamp);
    }
    inline uint32_t getSSRC() const
    {
        return ntohl(ssrc);
    }
    inline void setSSRC(uint32_t aSSRC)
    {
        ssrc = htonl(aSSRC);
    }
    inline uint16_t getExtId() const
    {
        return ntohs(extensionpayload);
    }
    inline void setExtId(uint16_t extensionId)
    {
        extensionpayload = htons(extensionId);
    }
    inline uint16_t getExtLength() const
    {
        return ntohs(extensionlength);
    }
    inline void setExtLength(uint16_t extensionLength)
    {
        extensionlength = htons(extensionLength);
    }
    inline int getHeaderLength()
    {
        return MIN_SIZE + cc * 4 + hasextension * (4 + ntohs(extensionlength) * 4);
    }
};

class AbsSendTimeExtension
{
public:
    uint32_t ext_info:8;
    uint32_t abs_data:24;
    inline uint8_t getId()
    {
        return ext_info >> 4;
    }
    inline uint8_t getLength()
    {
        return (ext_info & 0x0F);
    }
    inline uint32_t getAbsSendTime()
    {
        return ntohl(abs_data)>>8;
    }
    inline void setAbsSendTime(uint32_t aTime)
    {
        abs_data = htonl(aTime)>>8;
    }
};

class RtpRtxHeader
{
public:

    RtpHeader rtpHeader;
    uint16_t osn;

    inline uint16_t getOsn()
    {
        return ntohs (osn);
    }
    inline void setOs (uint16_t theOsn)
    {
        osn = htons (theOsn);
    }
};

class RtcpHeader
{
public:
    uint32_t blockcount :5;
    uint32_t padding :1;
    uint32_t version :2;
    uint32_t packettype :8;
    uint32_t length :16;
    uint32_t ssrc;
    union report_t
    {
        struct receiverReport_t
        {
            uint32_t ssrcsource;
            /* RECEIVER REPORT DATA*/
            uint32_t fractionlost:8;
            int32_t lost:24;
            uint32_t seqnumcycles:16;
            uint32_t highestseqnum:16;
            uint32_t jitter;
            uint32_t lastsr;
            uint32_t delaysincelast;
        } receiverReport;

        struct senderReport_t
        {
            uint64_t ntptimestamp;
            uint32_t rtprts;
            uint32_t packetsent;
            uint32_t octetssent;
            struct receiverReport_t rrlist[1];
        } senderReport;

        struct genericNack_t
        {
            uint32_t ssrcsource;
            uint32_t pid:16;
            uint32_t blp:16;
        } nackPacket;

        struct remb_t
        {
            uint32_t ssrcsource;
            uint32_t uniqueid;
            uint32_t numssrc:8;
            uint32_t brLength :24;
            uint32_t ssrcfeedb;

        } rembPacket;

        struct pli_t
        {
            uint32_t ssrcsource;
            uint32_t fci;
        } pli;

    } report;

    inline RtcpHeader()
        : 
        blockcount(0), 
        padding(0), 
        version(2), 
        packettype (0), 
        length(0),
        ssrc(0)
        {

        };

    inline bool isFeedback(void)
    {
        return (packettype==RTCP_Receiver_PT ||
                packettype==RTCP_PS_Feedback_PT ||
                packettype == RTCP_RTP_Feedback_PT);
    }
    inline bool isRtcp(void)
    {
        return (packettype == RTCP_Sender_PT ||
                packettype == RTCP_APP ||
                isFeedback());
    }
    inline uint8_t getPacketType()
    {
        return packettype;
    }
    inline void setPacketType(uint8_t pt)
    {
        packettype = pt;
    }
    inline uint8_t getBlockCount()
    {
        return (uint8_t)blockcount;
    }
    inline void setBlockCount(uint8_t count)
    {
        blockcount = count;
    }
    inline uint16_t getLength()
    {
        return ntohs(length);
    }
    inline void setLength(uint16_t theLength)
    {
        length = htons(theLength);
    }
    inline uint32_t getSSRC()
    {
        return ntohl(ssrc);
    }
    inline void setSSRC(uint32_t aSsrc)
    {
        ssrc = htonl(aSsrc);
    }
    inline uint32_t getSourceSSRC()
    {
        return ntohl(report.receiverReport.ssrcsource);
    }
    inline void setSourceSSRC(uint32_t sourceSsrc)
    {
        report.receiverReport.ssrcsource = htonl(sourceSsrc);
    }
    inline uint8_t getFractionLost()
    {
        return (uint8_t)report.receiverReport.fractionlost;
    }
    inline void setFractionLost(uint8_t fractionLost)
    {
        report.receiverReport.fractionlost = fractionLost;
    }
    inline uint32_t getLostPackets()
    {
        return ntohl(report.receiverReport.lost)>>8;
    }
    inline void setLostPackets(uint32_t lost)
    {
        report.receiverReport.lost = htonl(lost)>>8;
    }
    inline uint16_t getSeqnumCycles()
    {
        return ntohs(report.receiverReport.seqnumcycles);
    }
    inline void setSeqnumCycles(uint16_t seqnumcycles)
    {
        report.receiverReport.seqnumcycles = htons(seqnumcycles);
    }
    inline uint16_t getHighestSeqnum()
    {
        return ntohs(report.receiverReport.highestseqnum);
    }
    inline void setHighestSeqnum(uint16_t highest)
    {
        report.receiverReport.highestseqnum = htons(highest);
    }
    inline uint32_t getJitter()
    {
        return ntohl(report.receiverReport.jitter);
    }
    inline void setJitter(uint32_t jitter)
    {
        report.receiverReport.jitter = htonl(jitter);
    }
    inline uint32_t getLastSr()
    {
        return ntohl(report.receiverReport.lastsr);
    }
    inline void setLastSr(uint32_t lastsr)
    {
        report.receiverReport.lastsr = htonl(lastsr);
    }
    inline uint32_t getDelaySinceLastSr()
    {
        return ntohl (report.receiverReport.delaysincelast);
    }
    inline void setDelaySinceLastSr(uint32_t delaylastsr)
    {
        report.receiverReport.delaysincelast = htonl(delaylastsr);
    }

    inline uint32_t getPacketsSent()
    {
        return ntohl(report.senderReport.packetsent);
    }
    inline void setPacketsSent(uint32_t packetssent)
    {
        report.senderReport.packetsent = htonl(packetssent);
    }
    inline uint32_t getOctetsSent()
    {
        return ntohl(report.senderReport.octetssent);
    }
    inline uint64_t getNtpTimestamp()
    {
        return (((uint64_t)htonl(report.senderReport.ntptimestamp)) << 32) + htonl(report.senderReport.ntptimestamp >> 32);
    }
    inline uint16_t getNackPid()
    {
        return ntohs(report.nackPacket.pid);
    }
    inline void setNackPid(uint16_t pid)
    {
        report.nackPacket.pid = htons(pid);
    }
    inline uint16_t getNackBlp()
    {
        return report.nackPacket.blp;
    }
    inline void setNackBlp(uint16_t blp)
    {
        report.nackPacket.blp = blp;
    }
    inline void setREMBBitRate(uint64_t bitRate)
    {
        uint64_t max = 0x3FFFF; // 18 bits
        uint16_t exp = 0;
        while ( bitRate >= max && exp < 64)
        {
            exp+=1;
            max = max << 1;
        }
        uint64_t mantissa = bitRate >> exp;
        exp = exp&0x3F;
        mantissa = mantissa&0x3FFFF;
        uint32_t line = mantissa + (exp << 18);
        report.rembPacket.brLength = htonl(line)>>8;

    }
    inline uint32_t getBrExp()
    {
        //remove the 0s added by nothl (8) + the 18 bits of Mantissa
        return (ntohl(report.rembPacket.brLength)>>26);
    }
    inline uint32_t getBrMantis()
    {
        return (ntohl(report.rembPacket.brLength)>>8 & 0x3ffff);
    }
    inline uint8_t getREMBNumSSRC()
    {
        return report.rembPacket.numssrc;
    }
    inline void setREMBNumSSRC(uint8_t num)
    {
        report.rembPacket.numssrc = num;
    }
    inline uint32_t getREMBFeedSSRC()
    {
        return ntohl(report.rembPacket.ssrcfeedb);
    }
    inline void setREMBFeedSSRC(uint32_t ssrc)
    {
        report.rembPacket.ssrcfeedb = htonl(ssrc);
    }
    inline uint32_t getFCI()
    {
        return ntohl(report.pli.fci);
    }
    inline void setFCI(uint32_t fci)
    {
        report.pli.fci = htonl(fci);
    }

};

class FirHeader
{
public:
    uint32_t fmt :5;
    uint32_t padding :1;
    uint32_t version :2;
    uint32_t packettype :8;
    uint32_t length :16;
    uint32_t ssrc;
    uint32_t ssrcofmediasource;
    uint32_t ssrc_fir;
};

class RedHeader
{
public:
    uint32_t payloadtype :7;
    uint32_t follow :1;
    uint32_t tsLength :24;
    uint32_t getTS()
    {
        // remove the 8 bits added by nothl + the 10 from length
        return (ntohl(tsLength) & 0xfffc0000) >> 18;
    }
    uint32_t getLength()
    {
        return (ntohl(tsLength) & 0x3ff00);
    }
};

#endif // __RTP_HEADER_H__
