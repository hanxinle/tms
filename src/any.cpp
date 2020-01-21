#include "any.h"

namespace any
{

bool Any::GetInt(int64_t& val)
{
    if (! IsInt())
    {
        return false;
    }

    val = ToInt().val_;

    return true;
}

bool Any::GetDouble(double& val)
{
    if (! IsDouble())
    {
        return false;
    }

    val = ToDouble().val_;

    return true;
}

bool Any::GetString(std::string& val)
{
    if (! IsString())
    {
        return false;
    }

    val = ToString().val_;

    return true;
}

bool Any::GetVector(std::vector<Any*>& val)
{
    if (! IsVector())
    {
        return false;
    }

    val = ToVector().val_;
    
    return true;
}

bool Any::GetMap(std::map<std::string, Any*>& val)
{
    if (! IsMap())
    {
        return false;
    }

    val = ToMap().val_;

    return true;
}

Any::operator Int*()
{
    return (dynamic_cast<Int*>(this));
}

Any::operator Double*()
{
    return (dynamic_cast<Double*>(this));
}

Any::operator String*()
{
    return (dynamic_cast<String*>(this));
}

Any::operator Vector*()
{
    return (dynamic_cast<Vector*>(this));
}

Any::operator Map*()
{
    return (dynamic_cast<Map*>(this));
}

Int& Any::ToInt() const
{
    return *((Int*)this);
}

Double& Any::ToDouble() const
{
    return *((Double*)this);
}

String& Any::ToString() const
{
    return *((String*)this);
}

Vector& Any::ToVector() const
{
    return *((Vector*)this);
}

Map& Any::ToMap() const
{
    return *((Map*)this);
}

Any* Any::operator[](const size_t& index)
{
    return ToVector()[index];
}

Any* Any::operator[](const std::string& key)
{
    return ToMap()[key];
}

} // namespace any
