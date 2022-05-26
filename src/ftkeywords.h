#ifndef FTKEYWORDS_H
#define FTKEYWORDS_H

#include <string.h>
enum FtKeywordID {
  FTKW_NULL,
  FTKW_TITLE,
  FTKW_AUTHOR,
  FTKW_COPYRIGHT,
  FTKW_COMMENT,
  FTKW_MACHINE,
  FTKW_FRAMERATE,
  FTKW_EXPANSION,
  FTKW_VIBRATO,
  FTKW_SPLIT,
  FTKW_N163CHANNELS,
  FTKW_MACRO,
  FTKW_MACRON163,
  FTKW_INST2A03,
  FTKW_INSTN163,
  FTKW_N163WAVE,
  FTKW_TRACK,
  FTKW_COLUMNS,
  FTKW_ORDER,
  FTKW_PATTERN,
  FTKW_ROW,
};
struct FtKeyword {
  const char *name;
  enum FtKeywordID kwid;
};
const struct FtKeyword *
ftkw_lookup (register const char *str, register size_t len);

#endif
