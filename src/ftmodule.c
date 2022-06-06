#include "ftmodule.h"

const char *const FT_expansion_names[FT_NUM_ENVPOOLS] = {
  "VRC6", "VRC7", "FDS", "MMC5", "N163", "YM2149"
};

#define FT_2A03_NUM_CHANNELS 5
const unsigned char FT_expansion_channels[FT_NUM_ENVPOOLS] = {
  3, 6, 1, 2, 8, 3
};
