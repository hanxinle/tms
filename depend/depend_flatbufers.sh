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

flatbuffers_dir="flatbuffers"

if [[ ! -d "${depend_dir}/download/${flatbuffers_dir}" ]]; then
    echo "no flatbuffers dir and tar file found"
    mkdir ./download/${flatbuffers_dir}
    git clone https://github.com/google/flatbuffers.git ./download/${flatbuffers_dir}
fi

if [[ ! -f ${depend_dir}/bin/flatc ]]; then
    cd download/${flatbuffers_dir}
    cmake -G "Unix Makefiles"
    make -j 8 && cp ./flatc ${depend_dir}/bin && cp ./include/flatbuffers ${depend_dir}/include/ -r
    echo $?
    cd -
fi
