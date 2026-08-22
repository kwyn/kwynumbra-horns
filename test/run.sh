#!/bin/sh
# Golden-frame check for src/effects.cpp.
#   ./test/run.sh          diff current output against test/golden.txt
#   ./test/run.sh --bless  overwrite the baseline (only when a change is intended)
set -e
cd "$(dirname "$0")/.."

# env name changes with the board, so glob it — but not with `set --`, which
# would clobber $1 and silently break --bless.
FL=$(ls -d .pio/libdeps/*/FastLED/src 2>/dev/null | head -1)
OBJ=.pio/native-obj   # inside .pio, already gitignored

[ -d "$FL" ] || { echo "FastLED not fetched yet — run 'pio run' once first."; exit 1; }

# Build FastLED natively once, then cache. time_stub.cpp is skipped so
# golden.cpp can define its own millis(); compile_test.cpp defines a main().
if [ ! -d "$OBJ" ]; then
    echo "compiling FastLED for native (one time, ~1 min)..."
    mkdir -p "$OBJ"
    find "$FL" -name '*.cpp' ! -name 'time_stub.cpp' ! -name 'compile_test.cpp' \
      | xargs -P 8 -I{} sh -c \
        'g++ -std=c++17 -DFASTLED_STUB_IMPL -I'"$FL"' -c "$1" -o '"$OBJ"'/$(echo "$1" | tr "./" "__").o' _ {}
fi

g++ -std=c++17 -DFASTLED_STUB_IMPL -I"$FL" -Isrc \
    -o "$OBJ/golden" test/golden.cpp src/effects.cpp "$OBJ"/*.o

g++ -std=c++17 -Isrc -o "$OBJ/battery" test/battery_test.cpp
"$OBJ/battery"

# web/index.html has no build step and no linter, so this is its only check.
if command -v node > /dev/null 2>&1; then
    node test/web.js
else
    echo "SKIP - web checks need node"
fi

if [ "$1" = "--bless" ]; then
    "$OBJ/golden" > test/golden.txt
    echo "baseline updated: test/golden.txt"
    exit 0
fi

if "$OBJ/golden" | diff -u test/golden.txt - > /dev/null; then
    echo "PASS — effects output is byte-identical to the baseline"
else
    echo "FAIL — effects output changed:"
    "$OBJ/golden" | diff -u test/golden.txt - | head -20
    exit 1
fi
