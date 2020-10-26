#!/bin/bash
set -xeuo pipefail

dir=`pwd`
echo ${dir}

dir_obj=${dir}/obj

if  [[ ! -d ${dir_obj} ]]; then
    mkdir ${dir_obj}
fi

if [[ ! -f ${dir_obj}/lib/libsrtp2.a ]]; then
    unzip ${dir}/3rdparty/libsrtp-2.0.0.zip -d ${dir_obj}
    cd ${dir_obj}/libsrtp-2.0.0
    ./configure --prefix=${dir_obj} && make -j16 && make install
fi

if [[ ! -f ${dir_obj}/lib/libssl.a ]]; then
    unzip ${dir}/3rdparty/openssl-1.1.1g.zip -d ${dir_obj}
    cd ${dir_obj}/openssl-1.1.1g
    ./config enable-ssl-trace --prefix=${dir_obj} && make -j16 && make install
fi

if [[ ! -f ${dir_obj}/lib/libsrt.a ]]; then
    unzip ${dir}/3rdparty/srt-1.4.1.zip -d ${dir_obj}
    cd ${dir_obj}/srt-1.4.1
    PKG_CONFIG_PATH="${dir_obj}/lib/pkgconfig/" ./configure --prefix=${dir_obj} --disable-c++11 --disable-shared --enable-static \
        --enable-debug=0 --enable-heavy-logging=OFF \
        --openssl-include-dir=${dir_obj}/include/ \
        --openssl-ssl-library=${dir_obj}/lib/libssl.a \
        --openssl-crypto-library=${dir_obj}/lib/libcrypto.a \
        && make -j16 && make install
fi

if [[ ! -d ${dir_obj}/include/rapidjson ]]; then
    unzip ${dir}/3rdparty/rapidjson-1.1.0.zip -d ${dir_obj}
    cd ${dir_obj}/rapidjson-1.1.0
    mkdir build && cd build && cmake -DCMAKE_INSTALL_PREFIX:PATH=${dir_obj} ../ && make install
fi

cd ${dir}
