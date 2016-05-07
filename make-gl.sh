#!/bin/sh

{ cd ../vm ; ./make.sh || exit 1; cd ../engine ; }

#rm -rf build/debug-linux-`uname -m`/openarena.* build/debug-linux-`uname -m`/renderer build/debug-linux-`uname -m`/client

make -j8 debug V=1 \
USE_LOCAL_HEADERS=0 \
USE_OPENAL=1 USE_VOIP=1 USE_CURL=1 USE_CURL_DLOPEN=0 USE_CODEC_VORBIS=1 USE_MUMBLE=0 USE_FREETYPE=1 \
USE_RENDERER_DLOPEN=0 USE_INTERNAL_ZLIB=0 USE_INTERNAL_JPEG=1 BUILD_RENDERER_REND2=0 && \
cd build/debug-linux-`uname -m` && \
./openarena.`uname -m` +set r_fullscreen 0 $* #+devmap oacmpdm3

#gdb -ex run --args \
