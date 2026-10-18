#!/bin/sh
# Build taradino (Rise of the Triad) to WebAssembly in the taradino-emsdk image.
# Asyncify ignores indirect calls, except in the functions in
# web/asyncify-add.txt, where a wait does go through a function pointer (SDL's
# present, the menus' item routines, actors' think functions waiting for
# network input). Instrumenting every indirect call (1552 functions, SDL's
# audio callback, SDL_mixer and libADLMIDI's synthesis loops included) made
# iOS 26 kill the page some seconds into the sound.
# First: docker build -t taradino-emsdk -f web/Dockerfile.emsdk web  and  web/build-deps.sh
# Env: BUILD_TYPE (Release), SHAREWARE (ON), BUILD_DIR (build-wasm), EXTRA_LDFLAGS (e.g. --profiling-funcs),
#      IMAGE (taradino-emsdk), DEPS (web/deps)
set -e
WEB=$(cd "$(dirname "$0")" && pwd)
SRC=$(dirname "$WEB")
DEPS=${DEPS:-$WEB/deps}
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$SRC":/src -v "$DEPS":/deps -w /src ${IMAGE:-taradino-emsdk} sh -c "
emcmake cmake -G Ninja -B ${BUILD_DIR:-build-wasm} -DCMAKE_BUILD_TYPE=${BUILD_TYPE:-Release} -DTARADINO_SHAREWARE=${SHAREWARE:-ON} -DlibADLMIDI_DIR=/deps/lib/cmake/libADLMIDI \
 '-DCMAKE_EXE_LINKER_FLAGS=-lidbfs.js -sASYNCIFY -sASYNCIFY_IGNORE_INDIRECT -sASYNCIFY_ADD=@/src/web/asyncify-add.txt -sEMULATE_FUNCTION_POINTER_CASTS -sASYNCIFY_STACK_SIZE=262144 -sSTACK_SIZE=4MB -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sFORCE_FILESYSTEM -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sENVIRONMENT=web -sEXPORTED_RUNTIME_METHODS=FS,callMain,ENV ${EXTRA_LDFLAGS}' . >/dev/null
cmake --build ${BUILD_DIR:-build-wasm}"
