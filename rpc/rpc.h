#ifndef __RPC_H__
#define __RPC_H__

#include <sstream>

using std::ostringstream;

namespace protocol
{

const uint32_t kHeaderSize = 8;

class Serialize;
class Deserialize;

struct Rpc
{
	virtual void Write(Serialize& serialize) const = 0;
    virtual int Read(Deserialize& deserialize) = 0;
    void Dump(ostringstream& os) const
    {
        os << "Rpc: {}";
	}
};

} // namespace protocol

#endif // __RPC_H__
