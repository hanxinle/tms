#!/bin/bash

depend_dir="${HOME}/git/tms/depend"

if [ ! -d "${depend_dir}" ]; then
    mkdir "${depend_dir}"
fi

if [ ! -d "${depend_dir}/lib" ]; then
    mkdir "${depend_dir}/lib"
fi

if [ ! -d "${depend_dir}/include" ]; then
    mkdir "${depend_dir}/include"
fi

if [ ! -d "${depend_dir}/download" ]; then
    mkdir "${depend_dir}/download"
fi

if [ ! -d "${depend_dir}/tmp" ]; then
    mkdir "${depend_dir}/tmp"
fi

if [ ! -d "${depend_dir}/bin" ]; then
    mkdir "${depend_dir}/bin"
fi

sudo apt-get install autoconf libtool pkg-config tcl cmake  libbz2-dev zlib1g-dev

PATH="${depend_dir}/bin:$PATH"
PKG_CONFIG_PATH="${depend_dir}/lib/pkgconfig"

libsrtp_dir="libsrtp-v2.0.0";

if [[ ! -d "${depend_dir}/download/${libsrtp_dir}" && ! -f "${depend_dir}/download/${libsrtp_dir}.tar.gz" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${libsrtp_dir}
    wget https://github.com/cisco/libsrtp/archive/v2.0.0.tar.gz -O download/${libsrtp_dir}.tar.gz
    tar zxvf ./download/${libsrtp_dir}.tar.gz -C ./download/${libsrtp_dir} --strip-components 1
fi

if [ ! -d "${depend_dir}/download/${libsrtp_dir}" ]; then
    echo "no libsrtp dir found"
    mkdir ./download/${libsrtp_dir}
    tar zxvf ./download/${libsrtp_dir}.tar.gz -C ./download/${libsrtp_dir} --strip-components 1
fi

if [[ ! -f ${depend_dir}/lib/libsrtp2.a || ! -d ${depend_dir}/include/srtp2 ]]; then
    cd download/${libsrtp_dir}
    ./configure --prefix=${depend_dir} && make -j 8 && make install
    echo $?
    cd -
fi

libopenssl_dir="openssl-1.0.2g";

if [[ ! -d "${depend_dir}/download/${libopenssl_dir}" && ! -f "${depend_dir}/download/${libopenssl_dir}.tar.gz" ]]; then
    echo "no openssl dir and tar file found"
    mkdir ./download/${libopenssl_dir}
    wget https://www.openssl.org/source/openssl-1.0.2g.tar.gz -O download/${libopenssl_dir}.tar.gz
    tar zxvf ./download/${libopenssl_dir}.tar.gz -C ./download/${libopenssl_dir} --strip-components 1
fi

if [ ! -d "${depend_dir}/download/${libopenssl_dir}" ]; then
    echo "no openssl dir found"
    mkdir ./download/${libopenssl_dir}
    tar zxvf ./download/${libopenssl_dir}.tar.gz -C ./download/${libopenssl_dir} --strip-components 1
fi

if [[ ! -f ${depend_dir}/lib/libssl.a || ! -d ${depend_dir}/include/openssl ]]; then
    cd download/${libopenssl_dir}
    ./config --prefix=${depend_dir}/tmp --openssldir=${depend_dir}/tmp && make -j 8 && make install
    echo $?
    cp ${depend_dir}/tmp/include/openssl ${depend_dir}/include/openssl -r
    cp ${depend_dir}/tmp/lib/libssl.a ${depend_dir}/lib
    cp ${depend_dir}/tmp/lib/libcrypto.a ${depend_dir}/lib
    cd -
fi

librapidjson="rapidjson"

if [[ ! -d "${depend_dir}/download/${librapidjson}" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${librapidjson}
    git clone --depth 1 https://github.com/Tencent/rapidjson.git ./download/${librapidjson}
fi

if [[ ! -d "${depend_dir}/include/${librapidjson}" ]]; then
    cd download/${librapidjson}
	mkdir build
    cd build && cmake -DCMAKE_INSTALL_PREFIX:PATH=${depend_dir} ../ && make install
    echo $?
fi

libsrt="srt"

if [[ ! -d "${depend_dir}/download/${libsrt}" ]]; then
    echo "no ${libsrt} dir and tar file found"
    mkdir ./download/${libsrt}
    git clone --depth 1 https://github.com/Haivision/srt.git ./download/${libsrt}
fi

if [[ ! -d "${depend_dir}/include/${libsrt}" ]]; then
    cd download/${libsrt}
	./configure --prefix="${depend_dir}" --disable-c++11 --disable-shared --enable-static && \
	make -j4 && \
	make install
    echo $?
fi
