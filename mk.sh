#!/bin/sh
set -e
gcc -Wall -Wextra -Os -fsanitize=undefined -o mixer mixer.c canonwav.c
./mixer
paplay out.wav
