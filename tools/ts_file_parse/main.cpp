#include <fcntl.h>
#include <unistd.h>

#include <iostream>

#include "ts_reader.h"

using namespace std;

int TetTsReader(const char* file)
{
    int fd = open(file, O_RDONLY, 0664);
    if (fd < 0)
    {
        cout << "open " << file << " failed" << endl;
        return -1;
    }

    TsReader ts_reader;
    while (true)
    {
        uint8_t buf[188 * 64];
        int nbytes = read(fd, buf, sizeof(buf));
        if (nbytes == 0)
        {
            cout << "read EOF" << endl;
            break;
        }
        else if (nbytes < 0)
        {
            cout << "read error" << endl;
            break;
        }
        else
        {
            ts_reader.ParseTs(buf, nbytes);
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout << "Usage " << argv[0] << " xxx.ts" << endl;
        return -1;
    }

    return TetTsReader(argv[1]);
}
