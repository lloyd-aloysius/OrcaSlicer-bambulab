#!/bin/bash
set -euo pipefail
if [[ $# != 2 ]]; then
    echo "Usage: $0 OrcaSlicer.app EMPTY_OUTPUT_DIRECTORY" >&2
    exit 2
fi
app=$(cd "$1" && pwd)
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$2"
output=$(cd "$2" && pwd)
if [[ -n "$(ls -A "$output")" ]]; then
    echo "Choose an empty output directory: $output" >&2
    exit 1
fi
profile=$(mktemp -d)
trap 'rm -rf "$profile"' EXIT
cd "$output"
/usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    "$app/Contents/MacOS/OrcaSlicer" --datadir "$profile" --layer-change-gcode 'G92 E0' --arrange 1 --slice 1 \
    --outputdir "$output" --export-3mf 'Offline cube.gcode.3mf' "$root/tests/data/20mm_cube.obj"
python3 - "$output/Offline cube.gcode.3mf" <<'PY'
import hashlib
from pathlib import Path
import re
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as archive:
    gcode = archive.read("Metadata/plate_1.gcode")
    plain = Path(sys.argv[1]).with_name("plate_1.gcode").read_bytes()
    expected = archive.read("Metadata/plate_1.gcode.md5").decode().strip().lower()
    if plain != gcode or hashlib.md5(gcode).hexdigest() != expected:
        raise SystemExit("Standalone G-code and sliced-plate checksum differ")
    if not re.search(rb"^G1 [^;\n]*E[0-9.]+", gcode, re.M):
        raise SystemExit("No extrusion moves in sliced plate")
    print(f"Offline slice passed ({len(gcode)} bytes of G-code)")
PY
