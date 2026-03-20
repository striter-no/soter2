#!/usr/bin/bash

set -e

ABSOLUTE_IPREFIX="/usr/local"
INSTALL_LIB="${ABSOLUTE_IPREFIX}/lib"
INSTALL_INCLUDE="${ABSOLUTE_IPREFIX}/include"

# wget -O libsodium.tar https://github.com/jedisct1/libsodium/releases/download/1.0.21-RELEASE/libsodium-1.0.21.tar.gz

if [[ -e _libsodium ]]; then
    rm -rf ./_libsodium/*
fi

mkdir -p _libsodium
tar -xf ./libsodium.tar -C ./_libsodium
mv ./_libsodium/* ./__libsodium_temp
rm -rf ./_libsodium
mv ./__libsodium_temp ./_libsodium

cd ./_libsodium
CONFIGURE_ARGS="--prefix=${ABSOLUTE_IPREFIX} --libdir=${INSTALL_LIB} --includedir=${INSTALL_INCLUDE}"

ARCH="$(uname -m)"
if [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
    env CFLAGS="$CFLAGS -march=armv8-a+crypto+aes" ./configure $CONFIGURE_ARGS
else
    ./configure $CONFIGURE_ARGS
fi

make && make check 
make install

cd ..
rm -rf ./_libsodium
rm -rf ./libsodium.tar