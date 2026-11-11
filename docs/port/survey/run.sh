#!/bin/sh
# usage: penpen-run.sh <shell command>   (runs inside sandboxed container)
D=/Applications/Docker.app/Contents/Resources/bin/docker
exec $D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp \
  -v penpen-disc:/disc:ro -v penpen-work:/work \
  -v ${SCRIPTS:?set SCRIPTS to the dir holding these .py files}:/scripts:ro \
  penpen-tools sh -c "$1"
