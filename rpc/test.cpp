#include <iostream>

#include "deserialize.h"
#include "serialize.h"

using namespace protocol;
using namespace std;

int main(int argc, char* argv[])
{
    Serialize serialize;

    string name("xiaozhihong");
    string work("C++ programer");

    uint8_t age = 18;
    uint16_t height = 173;
    uint32_t weight = 50;
    uint64_t money =  11234;

    serialize.Write(name);
    serialize.Write(work);
    serialize.Write(age);
    serialize.Write(height);
    serialize.Write(weight);
    serialize.Write(money);

    string tmp;

    vector<uint32_t> vec_u32 = {1, 2, 3, 5, 6, 7};

    map<string, uint32_t> map_str_u32 = {{"xiao", 1}, {"zhi", 2}, {"hong", 3}};

    serialize.Write(vec_u32);

    serialize.Write(map_str_u32);

    serialize.WriteHeader(383658);

    vec_u32.clear();

    map_str_u32.clear();

    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;

    Deserialize deserialize(serialize.GetBuf(), serialize.GetCapacity());

    int ret = deserialize.Read(tmp);
    cout << "ret:" << ret << ",tmp:" << tmp << ",size:" << tmp.size() << endl;
    deserialize.Read(tmp);
    cout << "ret:" << ret << ",tmp:" << tmp << ",size:" << tmp.size() << endl;
    ret = deserialize.Read(age);
    cout << "ret:" << ret << ",age:" << (int)age << endl;
    ret = deserialize.Read(height);
    cout << "ret:" << ret << ",height:" << height << endl;
    ret = deserialize.Read(weight);
    cout << "ret:" << ret << ",weight:" << weight << endl;
    ret = deserialize.Read(money);
    cout << "ret:" << ret << ",money:" << money << endl;

    ret = deserialize.Read(vec_u32);
    cout << "ret:" << ret << endl;

    for (const auto& v : vec_u32)
    {
        cout << "v:" << v << endl;
    }
    
    ret = deserialize.Read(map_str_u32);
    cout << "ret:" << ret << endl;

    for (const auto& kv : map_str_u32)
    {
        cout << kv.first << "=>" << kv.second << endl;
    }

    uint32_t len = 0;
    ret = deserialize.ReadLen(len);
    cout << "ret:" << ret << ",len:" << len << endl;

    uint32_t protocol_id = 0;
    ret  = deserialize.ReadProtocolId(protocol_id);
    cout << "ret:" << ret << ",protocol_id:" << protocol_id << endl;
    
    return 0;
}
