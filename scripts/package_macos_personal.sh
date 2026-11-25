#!/bin/bash
# Package the native arm64 app for local use.
set -euo pipefail
if [[ $# != 2 ]]; then
    echo "Usage: $0 OrcaSlicer.app output.dmg" >&2
    exit 2
fi
app=$1
out=$2
binary="$app/Contents/MacOS/OrcaSlicer"
[[ -x "$binary" && -d "$app/Contents/Resources/profiles" ]]
[[ "$(plutil -extract CFBundleIdentifier raw "$app/Contents/Info.plist")" = com.lloydaloysius.OrcaSlicerPersonal ]]
for tool in "$binary" "$app/Contents/MacOS/orca-native-plugin-check"; do
    [[ -x "$tool" && "$(lipo -archs "$tool")" = arm64 ]]
    external=$(otool -L "$tool" | awk 'NR > 1 && $1 !~ "^(/System/Library/|/usr/lib/)" {print $1}')
    if [[ -n "$external" ]]; then
        echo "Non-system dependencies in $tool: $external" >&2
        exit 1
    fi
done
if [[ -e "$app/Contents/MacOS/libpjarczak_bambu_networking_bridge.dylib" ]]; then
    echo "Build the native bundle with build_release_macos.sh -Psx first." >&2
    exit 1
fi
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT
bundle="$staging/OrcaSlicer Personal.app"
ditto "$app" "$bundle"
codesign --force --sign - "$bundle"
codesign --verify --deep --strict "$bundle"
ln -s /Applications "$staging/Applications"
# hdiutil refuses to overwrite an existing image unless explicitly requested.
hdiutil create -volname "OrcaSlicer Personal" -format UDZO -srcfolder "$staging" "$out"
hdiutil verify "$out"
shasum -a 256 "$out"
