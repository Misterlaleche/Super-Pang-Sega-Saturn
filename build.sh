#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p cd_audio tools
cc -O2 Compiler/COMMON/JoEngineCueMaker.c -o tools/JoEngineCueMaker
cc -O2 Compiler/COMMON/JoEngineIsoToBin.c -o tools/JoEngineIsoToBin
chmod +x tools/mkisofs
make -j4 all
# PCM music and effects are files on the data track. No CD-DA tracks.
cat > game.cue <<'EOF'
FILE "game.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
    POSTGAP 00:02:00
EOF
