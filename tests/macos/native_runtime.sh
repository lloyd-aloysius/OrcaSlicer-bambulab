#!/bin/bash
# Exercise user-supplied native vendor libraries with synthetic local video.
set -euo pipefail
if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 NATIVE_PLUGIN_DIRECTORY [VENDOR_ZIP]" >&2
    exit 2
fi
plugins=$(cd "$1" && pwd)
archive=''
if [[ -n "${2:-}" ]]; then archive="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; fi
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
server=''
publisher=''
cleanup() {
    for pid in "$publisher" "$server"; do
        if [[ -n "$pid" ]]; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    rm -rf "$work"
}
trap cleanup EXIT
cd "$work"
xcrun clang++ -std=c++17 -I"$root/src" -framework AppKit "$root/src/slic3r/Utils/MacPluginCheck.cpp" -o probe
/usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' ./probe "$plugins" "$work" "$root/resources/cert"
if [[ -n "$archive" ]]; then
    xcrun clang++ -std=c++17 -dynamiclib -Wno-return-type-c-linkage -I"$root/src" -I"$root/deps_src" \
        '-DBAMBU_NETWORK_AGENT_VERSION="00.00.00.00"' "$root/tests/macos/fixture_network.cpp" -o incompatible.dylib
    xcrun clang++ -std=c++17 -I"$root/src" -framework Foundation "$root/src/slic3r/GUI/MacPluginInstall.mm" \
        "$root/tests/macos/native_install.cpp" -o install-test
    /usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' ./install-test "$archive" "$plugins" "$work/probe" "$work/profile" "$work/incompatible.dylib"
fi
xcrun clang++ -std=c++17 -fobjc-arc -I"$root/src" -framework AppKit -framework AVFoundation -framework QuartzCore \
    "$root/tests/macos/native_camera.mm" -o camera
python3 "$root/tests/macos/loopback_media.py" ./camera "$plugins/libBambuSource.dylib"

port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
cat > mediamtx.yml <<YAML
rtspAddress: 127.0.0.1:$port
rtspTransports: [tcp]
rtmp: false
hls: false
webrtc: false
srt: false
moq: false
paths:
  fixture: {}
YAML
mediamtx mediamtx.yml >server.log 2>&1 &
server=$!
for ((i=0; i<100; i++)); do
    if ! kill -0 "$server" 2>/dev/null; then cat server.log >&2; exit 1; fi
    if grep -q 'listener opened' server.log; then break; fi
    sleep 0.1
done
ffmpeg -nostdin -loglevel error -re -f lavfi -i testsrc2=size=320x240:rate=12 \
    -c:v libx264 -threads 1 -tune zerolatency -preset ultrafast -pix_fmt yuv420p -g 12 \
    -f rtsp -rtsp_transport tcp "rtsp://127.0.0.1:$port/fixture" >publisher.log 2>&1 &
publisher=$!
for ((i=0; i<100; i++)); do
    if ! kill -0 "$publisher" 2>/dev/null; then cat publisher.log >&2; exit 1; fi
    if grep -q 'publishing' server.log; then break; fi
    sleep 0.1
done
./camera "$plugins/libBambuSource.dylib" \
    "bambu:///rtsp___fixture:fixture@127.0.0.1:$port/fixture?proto=rtsp&device=fixture&net_ver=02.05.02.58&dev_ver=1&cli_id=test&cli_ver=01.10.01.50"
