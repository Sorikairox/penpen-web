#!/bin/sh
# S22 (DECOMP 23): hand-written synthetic jobs for type 0x30 (the draw, its posed nodes with motion keys: the motion
# reader is C since S22), type 0x32 (the draw) and the owner scene (construct, init, draw): CSD (cup4b) builds six
# type 0x30 and eleven type 0x32 actors that no route sees. Each SPEC is a difftest.c --hwsynth spec as tools/synth.py writes them,
# with the bases named (corpus snapshots that hold the objects: DECOMP 23) and
# the S21 `set ADDR VALUE` lines (a word of the base written, in prelude order; [X] = the base's word at X).
#   tools/synth/s22/run.sh SPEC NAME    SPEC = t30 | t30pose | t32 | owner
#                                       NAME = a NEW corpus directory /corpus/syn/NAME (never an existing one)
# Runs the hand build without generated code (/build/native-genlib/penpen: tools/s3.sh native with GEN=genlib) in
# the sandbox; prints only our vocabulary. DBG=1: why the first inputs of a job were refused (PENPEN_SYNTH_DEBUG).
set -eu
D=${DOCKER:-/Applications/Docker.app/Contents/Resources/bin/docker}
HERE=$(cd "$(dirname "$0")" && pwd)
sp=$1; nm=$2
case "$nm" in *[!a-z0-9_]*|'') echo "NAME: lower-case letters, digits, _"; exit 2 ;; esac
$D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp --user 1000:1000 \
  ${DBG:+-e PENPEN_SYNTH_DEBUG=1} -v penpen-corpus:/corpus:rw -v penpen-disc:/disc:ro -v penpen-build:/build:ro \
  -v "$HERE":/work:ro emscripten/emsdk:4.0.15-arm64 sh -c "[ -e /corpus/syn/$nm ] && { echo 'hw synth: /corpus/syn/$nm exists (a corpus directory is written once)'; exit 1; }; ${SYNTH_BIN:-/build/native-genlib/penpen} --disc /disc --verbose 0 --frame-log 0 --hwcorpus /corpus/syn/$nm:16 --hwsynth /work/$sp.spec" 2>&1 | grep -aE '^(hw |rt crash|boot stop)[ -~]*$'
