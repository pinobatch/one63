#!/bin/sh
set -e
mkdir -p build

# Don't turn on missing-field-initializers because the wordlist array
# in gperf's output contains missing field initializers for each hash
# table entry that does not correspond to a keyword.
CWARN="-Wall -Wextra -Wno-missing-field-initializers"

run_mixer()
{
  gcc $CWARN -Os -fsanitize=undefined -o mixer src/mixer.c src/canonwav.c
  ./mixer
  paplay out.wav
}

run_parser()
{
  gperf --output-file=build/ftkeywords.c src/ftkeywords.gperf
  gcc $CWARN -Os -fsanitize=address -o ftparse src/ftparse.c src/gaplist.c build/ftkeywords.c
  ./ftparse
}

run_parser
