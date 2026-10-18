#!/bin/sh
# Build jj_render, the program tools/jj_pack.py runs to pre-decode the
# pictures and render the music (the headless harness: the game itself), for
# Linux, Windows and macOS, with "zig cc" as cross compiler:
#   pip install ziglang
#   tools/build_renderers.sh            -> tools/bin/jj_render-<os>-<arch>
# TARGETS="triple:name ..." selects the targets.
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-tools/bin}
mkdir -p "$OUT"
list() { sed -n "/^$1 =/,/^\$/p" src/oj/sources.mk | sed "s/^$1 = //; s/\\\\//g" | tr '\n' ' '; }
OJ_FILES=$(list OJ_FILES)
XMP_FILES=$(list XMP_FILES)
EXT_FILES=$(list EXT_FILES)
OJ_DEFS=$(sed -n 's/^OJ_DEFS = //p' src/oj/sources.mk)
XMP_DEFS=$(sed -n 's/^XMP_DEFS = //p' src/oj/sources.mk)
EXT_DEFS=$(sed -n 's/^EXT_DEFS = //p' src/oj/sources.mk)
INC="-Isrc/port -Isrc/port/sdl -Isrc/oj -Isrc/ext/xmp -Isrc/ext/miniz -Isrc/ext/stb"
ZIG="${ZIG:-python3 -m ziglang}"
TARGETS="${TARGETS:-x86_64-linux-musl:linux-x86_64 aarch64-linux-musl:linux-arm64 x86_64-windows-gnu:windows-x86_64.exe x86_64-macos:macos-x86_64 aarch64-macos:macos-arm64}"
for target in $TARGETS; do
    triple=${target%%:*}
    name=jj_render-${target#*:}
    tmp=$(mktemp -d)
    objs=""
    n=0
    for f in $OJ_FILES; do
        n=$((n + 1))
        o="$tmp/oj$n.o"
        $ZIG c++ -target "$triple" -O2 -std=gnu++14 -DGNW $OJ_DEFS $EXT_DEFS -w -fno-rtti \
            $INC -include src/port/gnw_jj_glue.h -c "src/oj/$f" -o "$o" &
        objs="$objs $o"
    done
    for f in jj_core.cpp jj_sdl.cpp jj_cxx.cpp jj_sound.cpp jj_render.cpp jj_state.cpp; do
        o="$tmp/p_${f%.cpp}.o"
        $ZIG c++ -target "$triple" -O2 -std=gnu++14 -DGNW $OJ_DEFS $EXT_DEFS -w -fno-rtti \
            $INC -include src/port/gnw_jj_glue.h -c "src/port/$f" -o "$o" &
        objs="$objs $o"
    done
    wait
    for f in $XMP_FILES $EXT_FILES; do
        n=$((n + 1))
        o="$tmp/x$n.o"
        $ZIG cc -target "$triple" -O2 -std=gnu11 -DGNW $XMP_DEFS $EXT_DEFS -w $INC -c "src/ext/$f" -o "$o" &
        objs="$objs $o"
    done
    for f in src/port/jj_alloc.c src/port/jj_pack.c src/port/jj_file.c src/port/jj_bake.c src/port/jj_music.c harness/jj_host.c; do
        o="$tmp/c_$(basename "$f" .c).o"
        $ZIG cc -target "$triple" -O2 -std=gnu11 -DGNW -w $INC -c "$f" -o "$o" &
        objs="$objs $o"
    done
    wait
    $ZIG c++ -target "$triple" -O2 -s $objs -lm -o "$OUT/$name"
    rm -f "$OUT/${name%.exe}.pdb" "$OUT"/*.lib
    rm -rf "$tmp"
    echo "built $OUT/$name"
done
