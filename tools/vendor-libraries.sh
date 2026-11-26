#!/usr/bin/env bash
# Copies the firmware's Arduino libraries into All_in_One/src/ so the sketch
# builds the same way in the Arduino IDE and with arduino-cli, with nothing to
# install except the ESP32 board package. Run it only to upgrade a library:
#
#   tools/vendor-libraries.sh [path-to-Arduino-libraries]   (default ~/Documents/Arduino/libraries)
#
# then build every configuration (tools/build-all.sh), test on a board, and
# commit All_in_One/src/ together with the new src/VERSIONS.txt.
#
# One mechanical change is made to the copies: a library normally has its own
# folder on the compiler's include path, so it may include its files from
# anywhere as <ArduinoJson/...> or "inner.h". Inside a sketch's src/ folder it
# does not, so every include of a library's own file is rewritten to a path
# relative to the including file. Nothing else is changed.
set -euo pipefail
cd "$(dirname "$0")/.."

LIBS="${1:-$HOME/Documents/Arduino/libraries}"
DEST=All_in_One/src
NAMES="ArduinoJson MultiButton PubSubClient Rdm6300 SSLClient TinyGSM"

for name in $NAMES; do
  [ -f "$LIBS/$name/library.properties" ] || { echo "missing library: $LIBS/$name" >&2; exit 1; }
done

rm -rf "${DEST:?}"
mkdir -p "$DEST"
echo "# Libraries copied by tools/vendor-libraries.sh. Do not edit by hand." > "$DEST/VERSIONS.txt"

for name in $NAMES; do
  src="$LIBS/$name"
  version=$(grep -m1 '^version=' "$src/library.properties" | cut -d= -f2)
  url=$(grep -m1 '^url=' "$src/library.properties" | cut -d= -f2)
  mkdir -p "$DEST/$name"
  cp -R "$src/src/." "$DEST/$name/"
  cp "$src/$(ls "$src" | grep -iE '^licen[cs]e' | head -1)" "$DEST/$name/"
  rm -f "$DEST/$name/CMakeLists.txt"
  echo "$name $version $url" >> "$DEST/VERSIONS.txt"
done

python3 - "$DEST" $NAMES <<'PY'
import os, re, sys
dest, names = sys.argv[1], sys.argv[2:]
inc = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)[>"](.*)$')
total = 0
for name in names:
    root = os.path.join(dest, name)
    changed = 0
    for d, _, files in os.walk(root):
        for f in files:
            if not f.endswith((".c", ".cpp", ".h", ".hpp", ".tpp")):
                continue
            path = os.path.join(d, f)
            with open(path, errors="surrogateescape") as fh:
                lines = fh.readlines()
            out = []
            for line in lines:
                m = inc.match(line.rstrip("\n"))
                if m:
                    target = m.group(3)
                    beside = os.path.isfile(os.path.join(d, target))
                    at_root = os.path.isfile(os.path.join(root, target))
                    # An own file not reachable from here as written: quoted
                    # includes look beside the file, angle includes only on
                    # the include path, which won't contain the library.
                    if at_root and (m.group(2) == "<" or not beside):
                        rel = os.path.relpath(os.path.join(root, target), d)
                        line = '%s"%s"%s\n' % (m.group(1), rel, m.group(4))
                        changed += 1
                out.append(line)
            if out != lines:
                with open(path, "w", errors="surrogateescape") as fh:
                    fh.writelines(out)
    print("  %-13s %d includes made relative" % (name, changed))
    total += changed
PY

echo "Copied into $DEST:"
sed 1d "$DEST/VERSIONS.txt"
