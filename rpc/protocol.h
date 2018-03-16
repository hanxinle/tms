#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <iostream>
#include <sstream>

#include "deserialize.h"
#include "serialize.h"
#include "rpc.h"

using std::cout;
using std::endl;

//#define IF(cond) {int ret=cond;if(ret<0){cout<<__FILE__<<"#"<<__func__<<":"<<__LINE__<<endl;return ret;}}
#define IF(cond) {int ret=cond;if(ret<0){return ret;}}

using std::ostringstream;

#define WRITE_VECTOR(v,serialize)\
        serialize.Write((uint32_t)v.size());\
        for(const auto& item : v)\
        {\
            item.Write(serialize);\
        }\

#define READ_VECTOR(TYPE,v,deserialize)\
        uint32_t size = 0;\
        IF(deserialize.Read(size));\
        for (uint32_t i = 0; i != size; ++i)\
        {\
            TYPE item;\
            IF(item.Read(deserialize));\
            v.push_back(item);\
        }\

#define WRITE_SET(v,serialize)\
        serialize.Write((uint32_t)v.size());\
        for(const auto& item : v)\
        {\
            item.Write(serialize);\
        }\

#define READ_SET(TYPE,v,deserialize)\
        uint32_t size = 0;\
        IF(deserialize.Read(size));\
        for (uint32_t i = 0; i != size; ++i)\
        {\
            TYPE item;\
            IF(item.Read(deserialize));\
            v.insert(item);\
        }\


namespace protocol
{
    enum NodeType
    {
        RTMP_NODE = 1,
        MEDIA_CENTER = 2,
    };

    enum NodeRole
    {
        MASTER = 1,
        SLAVE = 2,
    };

    struct NodeInfo
    {
        NodeInfo()
            :
            ip(0),
            type(0),
            start_time_ms(0),
            pid(0)
        {
        }

        bool IsValid() const
        {
            return (ip != 0) && (!port.empty()) && (type != 0) && (start_time_ms != 0) && (pid != 0);
        }

        uint32_t ip;
        vector<uint16_t> port;
        uint8_t type;
        uint64_t start_time_ms;
        uint32_t pid;

        void Write(Serialize& serialize) const
        {
            serialize.Write(ip);
            serialize.Write(port);
            serialize.Write(type);
            serialize.Write(start_time_ms);
            serialize.Write(pid);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(ip));
            IF(deserialize.Read(port));
            IF(deserialize.Read(type));
            IF(deserialize.Read(start_time_ms));
            IF(deserialize.Read(pid));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "NodeInfo: {ip:" << ip << ",port:";

            for (const auto& v : port)
            {
                os << v << ",";
            }

            os << ",type:" << (int)type << ",start_time_ms:" << start_time_ms << ",pid:" << pid << "}";
        }

        bool inline operator<(const NodeInfo& other) const
        {
            if (ip != other.ip)
            {
                return true;
            }

            if (port.size() != other.port.size())
            {
                return true;
            }
            else
            {
                auto l_iter = port.begin();
                auto r_iter = other.port.begin();

                for ( ; l_iter != port.end() && r_iter != other.port.end(); ++l_iter, ++r_iter)
                {
                    if (*l_iter != *r_iter)
                    {
                        return true;
                    }
                }
            }

            if (type != other.type)
            {
                return true;
            }

            if (start_time_ms != other.start_time_ms)
            {
                return true;
            }

            if (pid != other.pid)
            {
                return true;
            }

            return false;
        }

        bool inline operator!=(const NodeInfo& other)
        {
            return !((*this)<other) && !(other<(*this));
        }

    };


    struct NodeRegisterReq : public Rpc
    {
        enum {protocol_id = 1};

        NodeInfo node_info;
        uint64_t req_time;

        void Write(Serialize& serialize) const
        {
            node_info.Write(serialize);
            serialize.Write(req_time);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(node_info.Read(deserialize));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "NodeRegisterReq: {";
            node_info.Dump(os);

            os << ",req_time:" << req_time;
            os << "}";
        }
    };

    struct NodeRegisterRsp : public Rpc
    {
        enum {protocol_id = 2};

        uint64_t rsp_time;

        void Write(Serialize& serialize) const
        {
            serialize.Write(rsp_time);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(rsp_time));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "NodeRegisterRsp: {";
            os << "rsp_time:" << rsp_time;
            os << "}";
        }
    };

    struct GetNodeListReq : public Rpc
    {
        enum {protocol_id = 3};

        uint64_t req_time;
        uint32_t type;
        uint16_t node_number;

        void Write(Serialize& serialize) const
        {
            serialize.Write(req_time);
            serialize.Write(type);
            serialize.Write(node_number);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(req_time));
            IF(deserialize.Read(type));
            IF(deserialize.Read(node_number));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "GetNodeListReq: {"
               << "req_time:" << req_time
               << ",type:" << type
               << ",node_number:" << node_number
               << "}";
        }
    };

    struct GetNodeListRsp : public Rpc
    {
        enum {protocol_id = 4};

        uint64_t rsp_time;
        uint32_t type;
        set<NodeInfo> node_infos;

        void Write(Serialize& serialize) const
        {
            serialize.Write(rsp_time);
            serialize.Write(type);
            WRITE_SET(node_infos, serialize);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(rsp_time));
            IF(deserialize.Read(type));
            READ_SET(NodeInfo, node_infos, deserialize);

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "GetNodeListRsp: {"
               << "rsp_time:" << rsp_time 
               << ",type:" << type 
               << ",node_infos.size():" << node_infos.size();

            for (const auto& node : node_infos)
            {
                os << "[";
                node.Dump(os);
                os << "]";
            }

            os << "}";
        }
    };

    struct StreamInfo
    {
        string stream;
        string app;
        NodeInfo publish_node;

        void Write(Serialize& serialize) const
        {
            serialize.Write(stream);
            serialize.Write(app);
            publish_node.Write(serialize);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(stream))
            IF(deserialize.Read(app))
            IF(publish_node.Read(deserialize))

            return 0;
        }


        void Dump(ostringstream& os) const
        {
            os << "StreamInfo: {";
            os << "stream:" << stream
               << ",app:" << app
               << ",";

            publish_node.Dump(os);

            os << "}";
        }
    };

    struct StreamRegisterReq : public Rpc
    {
        enum {protocol_id = 201};

        NodeInfo node_info;
        vector<StreamInfo> stream_infos;
        uint16_t role;
        uint64_t req_time;

        void Write(Serialize& serialize) const
        {
            node_info.Write(serialize);
            WRITE_VECTOR(stream_infos, serialize);
            serialize.Write(role);
            serialize.Write(req_time);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            node_info.Read(deserialize);
            READ_VECTOR(StreamInfo, stream_infos, deserialize);
            IF(deserialize.Read(role));
            IF(deserialize.Read(req_time));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "StreamRegisterReq: {";

            for (const auto& stream : stream_infos)
            {
                os << "[";
                stream.Dump(os);
                os << "]";
            }
            os << ",";
            node_info.Dump(os);
            os << ",role:" << role;
            os << ",req_time:" << req_time;
            os << "}";
        }
    };  // StreamRegisterReq

    struct StreamRegisterRsp : public Rpc
    {
        enum {protocol_id = 202};

        uint64_t rsp_time;

        void Write(Serialize& serialize) const
        {
            serialize.Write(rsp_time);
            serialize.WriteHeader(protocol_id);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(rsp_time));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "StreamRegisterRsp: {";
            os << "rsp_time:" << rsp_time;
            os << "}";
        }
    };

    struct GetAppStreamMasterNodeReq : public Rpc
    {
        enum {protocol_id = 203};

        uint64_t req_time;
        string app;
        string stream;

        void Write(Serialize& serialize) const
        {
            serialize.Write(req_time);
            serialize.Write(app);
            serialize.Write(stream);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(req_time));
            IF(deserialize.Read(app));
            IF(deserialize.Read(stream));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "GetAppStreamMasterNodeReq: {"
               << "req_time:" << req_time
               << ",app:" << app
               << ",stream:" << stream
               << "}";
        }
    };

    struct GetAppStreamMasterNodeRsp : public Rpc
    {
        enum {protocol_id = 204};

        uint64_t rsp_time;
        string app;
        string stream;
        NodeInfo node_info;

        void Write(Serialize& serialize) const
        {
            serialize.Write(rsp_time);
            serialize.Write(app);
            serialize.Write(stream);
            node_info.Write(serialize);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(rsp_time));
            IF(deserialize.Read(app));
            IF(deserialize.Read(stream));
            IF(node_info.Read(deserialize));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "GetAppStreamMasterNodeRsp: {"
               << "rsp_time:" << rsp_time
               << ",app:" << app
               << ",stream:" << stream
               << ",";

            node_info.Dump(os);

            os << "}";
        }
    };

    struct CreateVideoTranscodeReq
    {
        enum {protocol_id = 3000};

        uint64_t req_time;
        string app;
        string stream;
        uint32_t bit_rate;
        uint32_t width;
        uint32_t height;
        uint16_t fps;

        void Write(Serialize& serialize) const
        {
            serialize.Write(req_time);
            serialize.Write(app);
            serialize.Write(stream);
            serialize.Write(bit_rate);
            serialize.Write(width);
            serialize.Write(height);
            serialize.Write(fps);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(req_time));
            IF(deserialize.Read(app));
            IF(deserialize.Read(stream));
            IF(deserialize.Read(bit_rate));
            IF(deserialize.Read(width));
            IF(deserialize.Read(height));
            IF(deserialize.Read(fps));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "CreateVideoTranscodeReq: {"
               << "req_time:" << req_time
               << ",app:" << app
               << ",stream:" << stream
               << ",bit_rate:" << bit_rate
               << ",width:" << width
               << ",height:" << height
               << ",fps:" << fps;

            os << "}";
        }
    };

    struct CreateVideoTranscodeRsp
    {
        enum {protocol_id = 3001};

        uint64_t rsp_time;
        string app;
        string stream;
        uint32_t rsp_code;

        void Write(Serialize& serialize) const
        {
            serialize.Write(rsp_time);
            serialize.Write(app);
            serialize.Write(stream);
            serialize.Write(rsp_code);
            serialize.WriteHeader(protocol_id);
        }

        int Read(Deserialize& deserialize)
        {
            IF(deserialize.Read(rsp_time));
            IF(deserialize.Read(app));
            IF(deserialize.Read(stream));
            IF(deserialize.Read(rsp_code));

            return 0;
        }

        void Dump(ostringstream& os) const
        {
            os << "CreateVideoTranscodeReq: {"
               << "rsp_time:" << rsp_time
               << ",app:" << app
               << ",stream:" << stream
               << ",rsp_code:" << rsp_code;

            os << "}";
        }
    };

} // namespace protocol


#endif // __PROTOCOL_H__
