#!/bin/sh
# Render-regression check for cfl-mii, on a PC, in seconds: builds the same Miis with upstream cfl-mii (UPSTREAM below)
# and with this working tree, against a stand-in GPU that records every draw and the state that decides its pixels
# (tests/trace/fake), and compares them Mii by Mii. The working tree is checked twice: through the original API
# (CFL_InitCharModel, CFL_CommandMakeModelIcon) and, when it has it, the per-frame API (CFL_BeginCharModel,
# CFL_StepCharModel, CFL_DrawModelIcon). Identical draws mean identical pixels on the console.
#
#   tests/trace/run.sh CFL_Res.dat [count] [seed] [-- extra cfl-trace options, e.g. --real CFL_DB.dat]
#
# CFL_Res.dat is Nintendo's file: dump it from your own console and keep it out of the repository. count defaults to
# 5000, seed to 1. Needs git and a C compiler ($CC, default cc). On a mismatch it prints the first Mii that differs and
# the first lines where its two traces part (the full traces are left in the work folder it names).
#
# Expected differences: none, unless a ubll.lst is passed (--ubll): upstream asks for 16,000 bytes of a 12,000-byte
# file and never applies the blocklist, which this tree fixes.
set -eu
UPSTREAM=b85be4d66e9c89d203840cbe7fa0f638a44cdde2   # jamesrhg/cfl-mii main, 2026-11 (the fork's starting point)
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
res=${1:?usage: run.sh CFL_Res.dat [count] [seed] [-- options]}
[ -f "$res" ] || { echo "run.sh: no such file: $res" >&2; exit 2; }
res=$(cd "$(dirname "$res")" && pwd)/$(basename "$res")
shift
count=5000; seed=1
if [ $# -gt 0 ] && [ "$1" != "--" ]; then count=$1; shift; fi
if [ $# -gt 0 ] && [ "$1" != "--" ]; then seed=$1; shift; fi
if [ $# -gt 0 ] && [ "$1" = "--" ]; then shift; fi
CC=${CC:-cc}
work=${CFL_TRACE_WORK:-$(mktemp -d "${TMPDIR:-/tmp}/cfl-trace.XXXXXX")}
mkdir -p "$work/upstream-src"
git -C "$repo" archive "$UPSTREAM" source include | tar -x -C "$work/upstream-src"

build() {   # build NAME SOURCE_ROOT
    "$CC" -O2 -std=gnu11 -w -I"$here/fake" -I"$2/include" -include "$here/fake/compat.h" \
        "$2/source/cfl_mii.c" "$here/harness.c" "$here/fake/fake.c" -lm -o "$work/$1"
}
build upstream "$work/upstream-src"
build current "$repo"

run() {   # run NAME [options]: the summary lines into NAME.txt and the exit status into NAME.rc (in the background)
    name=$1; shift
    ( rc=0; "$work/$name" "$res" --count "$count" --seed "$seed" "$@" > "$work/$name.txt" || rc=$?; echo $rc > "$work/$name.rc" ) &
}
start=$(date +%s)
run upstream "$@"
run current "$@"
paths="current"
if grep -q CFL_HAS_FRAME_STEPS "$repo/include/cfl_mii.h"; then
    cp "$work/current" "$work/frames"
    run frames --frames "$@"
    paths="current frames"
fi
wait
for name in upstream $paths; do   # a build that failed or built nothing compares equal to another: never a pass
    rc=$(cat "$work/$name.rc")
    if [ "$rc" != 0 ] || [ ! -s "$work/$name.txt" ]; then
        echo "$name: cfl-trace failed (exit $rc, $(wc -l < "$work/$name.txt" | tr -d ' ') Miis); see the error above" >&2
        exit 2
    fi
done
echo "$(wc -l < "$work/upstream.txt" | tr -d ' ') Miis, built $(echo $paths | wc -w | tr -d ' ')+1 ways, in $(( $(date +%s) - start )) s"

status=0
for name in $paths; do
    if cmp -s "$work/upstream.txt" "$work/$name.txt"; then
        echo "$name: identical to upstream ($(awk '{n += $7} END {print n}' "$work/$name.txt") draws)"
        continue
    fi
    status=1
    first=$(diff "$work/upstream.txt" "$work/$name.txt" | sed -n 's/^< \([0-9]*\) .*/\1/p' | head -1)
    differ=$(diff "$work/upstream.txt" "$work/$name.txt" | grep -c '^<' || true)
    echo "$name: $differ Miis differ from upstream; the first is Mii $first"
    grep "^$first " "$work/upstream.txt" | sed 's/^/  upstream: /'
    grep "^$first " "$work/$name.txt" | sed "s/^/  $name:  /"
    flag=; [ "$name" = frames ] && flag=--frames
    "$work/upstream" "$res" --count "$count" --seed "$seed" --dump "$first" "$@" > "$work/upstream-$first.trace"
    "$work/$name" "$res" --count "$count" --seed "$seed" --dump "$first" $flag "$@" > "$work/$name-$first.trace"
    diff "$work/upstream-$first.trace" "$work/$name-$first.trace" | head -40 || true
done
echo "work folder: $work"
exit $status
