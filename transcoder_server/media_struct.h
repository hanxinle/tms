#ifndef __MEDIA_STRUCT_H__
#define __MEDIA_STRUCT_H__

#include <string>

using std::string;

struct TsMedia
{
    TsMedia()
        :   
        duration(0),
        first_dts(0)
    {   
    }   

    double duration;
    double first_dts;
    string ts_data;
};

#endif // __MEDIA_STRUCT_H__
