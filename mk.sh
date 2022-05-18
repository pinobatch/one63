#!/bin/sh
set -e
gcc -Wall -Wextra -Os -fsanitize=undefined -o mixer src/mixer.c src/canonwav.c
./mixer
paplay out.wav
