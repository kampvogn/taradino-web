#!/bin/sh
# Build libADLMIDI (static, DOSBox OPL3 emulator only) into web/deps for the WebAssembly build.
# Env: IMAGE (taradino-emsdk, from Dockerfile.emsdk)
set -e
WEB=$(cd "$(dirname "$0")" && pwd)
COMMIT=9c2c093fe3aa0eb24e400115fa7d5cf50355ea48
[ -d "$WEB/libadlmidi" ] || git clone -q https://github.com/Wohlstand/libADLMIDI "$WEB/libadlmidi"
git -C "$WEB/libadlmidi" checkout -q $COMMIT
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$WEB":/w -w /w/libadlmidi ${IMAGE:-taradino-emsdk} sh -c "
emcmake cmake -G Ninja -B build-wasm -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/w/deps \
 -DlibADLMIDI_STATIC=ON -DlibADLMIDI_SHARED=OFF -DWITH_EMBEDDED_BANKS=ON -DWITH_MIDI_SEQUENCER=ON -DWITH_XMI_SUPPORT=ON \
 -DUSE_DOSBOX_EMULATOR=ON -DUSE_NUKED_EMULATOR=OFF -DUSE_OPAL_EMULATOR=OFF -DUSE_JAVA_EMULATOR=OFF -DUSE_ESFMU_EMULATOR=OFF \
 -DUSE_MAME_EMULATOR=OFF -DUSE_YMFM_EMULATOR=OFF -DUSE_NUKED_OPL2_LLE_EMULATOR=OFF -DUSE_NUKED_OPL3_LLE_EMULATOR=OFF \
 -DWITH_MIDIPLAY=OFF -DWITH_UNIT_TESTS=OFF -DWITH_VLC_PLUGIN=OFF -DWITH_HQ_RESAMPLER=OFF -DWITH_GENADLDATA=OFF . >/dev/null
cmake --build build-wasm --target install >/dev/null"
echo "libADLMIDI installed in $WEB/deps"
