#!/bin/sh
set -e
mkdir -p build

run_mixer()
{
  gcc -Wall -Wextra -Os -fsanitize=undefined -o mixer src/mixer.c src/canonwav.c
  ./mixer
  paplay out.wav
}

run_parser()
{
  gperf --output-file=build/ftkeywords.c src/ftkeywords.gperf
  less build/ftkeywords.c
}

run_mixer
