#!/bin/bash

depend_dir="${HOME}/git/Trs/depend"

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

#sudo apt-get install autoconf libtool pkg-config 

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

nasm_dir="nasm-2.13.02"

if [[ ! -d "${depend_dir}/download/${nasm_dir}" && ! -f "${depend_dir}/download/${nasm_dir}.tar.gz" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${nasm_dir}
    wget http://www.nasm.us/pub/nasm/releasebuilds/2.13.02/nasm-2.13.02.tar.bz2 -O download/${nasm_dir}.tar.bz2
    tar jxvf ./download/${nasm_dir}.tar.bz2 -C ./download/${nasm_dir} --strip-components 1
fi

if [ ! -d "${depend_dir}/download/${nasm_dir}" ]; then
    echo "no libsrtp dir found"
    mkdir ./download/${nasm_dir}
    tar jxvf ./download/${nasm_dir}.tar.bz2 -C ./download/${nasm_dir} --strip-components 1
fi

if [[ ! -f ${depend_dir}/bin/nasm ]]; then
    cd download/${nasm_dir}
    ./autogen.sh
    ./configure --prefix=${depend_dir} --bindir=${depend_dir}/bin && make -j 8 && make install
    echo $?
    cd -
fi

libx264_dir="libx264"

if [[ ! -d "${depend_dir}/download/${libx264_dir}" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${libx264_dir}
    #git clone http://git.videolan.org/git/x264.git ./download/${libx264_dir}
    git clone https://github.com/mirror/x264.git ./download/${libx264_dir}
fi

if [[ ! -f ${depend_dir}/lib/libx264.a ]]; then
    cd download/${libx264_dir}
    ./configure --prefix=${depend_dir} --bindir=${depend_dir}/bin --enable-static && make -j 8 && make install
    echo $?
    cd -
fi

libvpx_dir="libvpx"

if [[ ! -d "${depend_dir}/download/${libvpx_dir}" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${libvpx_dir}
    git clone --depth 1 https://chromium.googlesource.com/webm/libvpx.git ./download/${libvpx_dir}
fi

if [[ ! -f ${depend_dir}/lib/libvpx.a ]]; then
    cd download/${libvpx_dir}
    ./configure --prefix=${depend_dir} --disable-examples --disable-unit-tests --enable-vp9-highbitdepth --as=nasm && make -j 8 && make install
    echo $?
    cd -
fi

libfdk_aac_dir="libfdk-aac"

if [[ ! -d "${depend_dir}/download/${libfdk_aac_dir}" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${libfdk_aac_dir}
    git clone --depth 1 https://github.com/mstorsjo/fdk-aac ./download/${libfdk_aac_dir}
fi

if [[ ! -f ${depend_dir}/lib/libfdk-aac.a ]]; then
    cd download/${libfdk_aac_dir}
    autoreconf -fiv && ./configure --prefix=${depend_dir} --disable-shared && make -j 8 && make install
    echo $?
    cd -
fi

libopus_dir="libopus"

if [[ ! -d "${depend_dir}/download/${libopus_dir}" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${libopus_dir}
    git clone --depth 1 https://github.com/xiph/opus.git ./download/${libopus_dir}
fi

if [[ ! -f ${depend_dir}/lib/libopus.a ]]; then
    cd download/${libopus_dir}
    ./autogen.sh && ./configure --prefix=${depend_dir} --disable-shared && make -j 8 && make install
    echo $?
    cd -
fi

ffmpeg_dir="ffmpeg"

if [[ ! -d "${depend_dir}/download/${ffmpeg_dir}" && ! -f "${depend_dir}/download/${ffmpeg_dir}.tar.gz" ]]; then
    echo "no libsrtp dir and tar file found"
    mkdir ./download/${ffmpeg_dir}
    wget http://ffmpeg.org/releases/ffmpeg-3.4.2.tar.bz2 -O download/${ffmpeg_dir}.tar.bz2
    tar jxvf ./download/${ffmpeg_dir}.tar.bz2 -C ./download/${ffmpeg_dir} --strip-components 1
fi

if [ ! -d "${depend_dir}/download/${ffmpeg_dir}" ]; then
    echo "no libsrtp dir found"
    mkdir ./download/${ffmpeg_dir}
    tar jxvf ./download/${ffmpeg_dir}.tar.bz2 -C ./download/${ffmpeg_dir} --strip-components 1
fi

if [[ ! -f ${depend_dir}/lib/libavcodec.a ]]; then
    cd download/${ffmpeg_dir}
    echo $PKG_CONFIG_PATH
    PKG_CONFIG_PATH="${depend_dir}/lib/pkgconfig" ./configure --prefix=${depend_dir} --pkg-config-flags="--static" --extra-cflags="-I${depend_dir}/include" --extra-ldflags="-L${depend_dir}/lib" \
                --extra-libs="-lpthread -lm" --bindir="${depend_dir}/bin" --enable-gpl --enable-libfdk-aac \
                --enable-libopus --enable-libvpx --enable-libx264 --enable-nonfree --disable-vaapi --disable-vdpau && make -j 8 && make install
    echo $?
    cd -
fi
