#!/bin/sh
# Put the page, both builds and Apogee's shareware package together in web/stage
# (serve it with any static web server; this is also what the tests use).
set -e
WEB=$(cd "$(dirname "$0")" && pwd)
SRC=$(dirname "$WEB")
ST=${STAGE:-$WEB/stage}
mkdir -p "$ST/full" "$ST/data"
cp "$WEB/index.html" "$ST/"
cp "$SRC/build-wasm/taradino.js" "$SRC/build-wasm/taradino.wasm" "$ST/"
if [ -f "$SRC/build-full/taradino.wasm" ]; then cp "$SRC/build-full/taradino.js" "$SRC/build-full/taradino.wasm" "$ST/full/"; fi
# The shareware episode, unchanged as its licence requires (VENDOR.DOC)
Z="$ST/data/1rott13.zip"
[ -f "$Z" ] || curl -sfo "$Z" https://icculus.org/rott/share/1rott13.zip
echo "eea67a85845d52fa0ee7272e3ae24b79769ed1e8bdff7b1360904078bc51ad1e  $Z" | sha256sum -c --quiet
echo "staged in $ST"
