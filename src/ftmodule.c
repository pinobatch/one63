#include "ftmodule.h"
#include <stdlib.h>

#define EXPECTED_INSTS 16
#define EXPECTED_ENVS_PER_INST 2
#define EXPECTED_SONGS 16
#define EXPECTED_ORDER 16
#define EXPECTED_PATTERNS 16

const char *const FT_expansion_names[FT_NUM_ENVPOOLS] = {
  "VRC6", "VRC7", "FDS", "MMC5", "N163", "YM2149"
};

#define FT_2A03_NUM_CHANNELS 5
const unsigned char FT_expansion_channels[FT_NUM_ENVPOOLS] = {
  3, 6, 1, 2, 8, 3
};

void FTSong_unlink(FTSong *song) {
  if (!song) return;
  free(song->title);
  Gap_delete(song->order);  // a list of fixed-length rows
  while (!Gap_isEmpty(song->patterns)) {
    GapList **result = Gap_get(song->patterns, 0);
    GapList *trackPatterns = *result;
    Gap_delete(trackPatterns);
    Gap_remove(song->patterns, 0);
  }
  Gap_delete(song->patterns);
  song->title = 0;
  song->order = 0;
  song->patterns = 0;
}

int FTSong_init(FTSong *song, size_t nchannels, size_t rows_per_pattern) {
  if (rows_per_pattern > FTPAT_MAX_ROWS) return -1;
  song->title = NULL;
  song->order = Gap_new(nchannels, EXPECTED_ORDER);
  song->patterns = Gap_new(sizeof(GapList *), nchannels);
  song->rows_per_pattern = rows_per_pattern;
  song->start_speed = 6;
  song->start_tempo = 150;
  if (!song->order || !song->patterns) goto on_bad_alloc;
  for (size_t i = 0; i < nchannels; ++i) {
    GapList *trackPatterns = Gap_new(rows_per_pattern * sizeof(FTPatRow), EXPECTED_PATTERNS);
    if (!trackPatterns) goto on_bad_alloc;
    if (!Gap_add(song->patterns, &trackPatterns)) goto on_bad_alloc;
  }
  return 0;

on_bad_alloc:
  FTSong_unlink(song);
  return -1;
}

void FTModule_delete(FTModule *module) {
  if (!module) return;
  free(module->title);
  free(module->author);
  free(module->copyright);
  // Delete instruments
  if (module->instruments) {
    while (!Gap_isEmpty(module->instruments)) {
      FTPSGInstrument *ws = Gap_get(module->instruments, 0);
      Gap_delete(ws->waves);
      Gap_remove(module->instruments, 0);
    }
    Gap_delete(module->instruments);
  }
  // Delete envelopes
  if (module->all_envelopes) {
    while (!Gap_isEmpty(module->all_envelopes)) {
      FTEnvelope **ws = Gap_get(module->all_envelopes, 0);
      free(*ws);
      Gap_remove(module->all_envelopes, 0);
    }
    Gap_delete(module->all_envelopes);
  }
  // Delete songs
  if (module->songs) {
    while (!Gap_isEmpty(module->songs)) {
      FTSong *song = Gap_get(module->songs, 0);
      FTSong_unlink(song);
      Gap_remove(module->songs, 0);
    }
    Gap_delete(module->songs);
  }
  free(module);
}

FTModule *FTModule_new(void) {
  FTModule *module = calloc(1, sizeof(FTModule));
  if (!module) return 0;
  // Clear pointers manually (in case (void *)0 isn't bitwise 0)
  module->title = 0;
  module->author = 0;
  module->copyright = 0;
  // Allocate dynamic arrays
  module->instruments = Gap_new(sizeof(FTPSGInstrument), EXPECTED_INSTS);
  module->all_envelopes
    = Gap_new(sizeof(FTEnvelope *), EXPECTED_INSTS * EXPECTED_ENVS_PER_INST);
  module->songs = Gap_new(sizeof(FTSong), EXPECTED_SONGS);
  if (!module->songs || !module->instruments || !module->all_envelopes) {
    FTModule_delete(module);
    return 0;
  }
  return module;
}

size_t FTModule_count_channels(unsigned int expansion) {
  size_t count = FT_2A03_NUM_CHANNELS;
  for (size_t i = 0; i < FT_NUM_ENVPOOLS; ++i) {
    if (expansion & (1 << i)) count += FT_expansion_channels[i];
  }
  return count;
}

FTPSGInstrument *FTModule_get_instrument(FTModule *module, size_t instid) {
  if (!module || !module->instruments) return 0;
  static const FTPSGInstrument null_instrument =
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
  while (Gap_size(module->instruments) <= instid) {
    if (!Gap_add(module->instruments, &null_instrument)) return 0;
  }
  return Gap_get(module->instruments, instid);
}

unsigned char *FTModule_get_wave(FTModule *module, size_t instid,
                                 size_t waveid, size_t *elSize) {
  FTPSGInstrument *inst = FTModule_get_instrument(module, instid);
  if (!inst || !inst->waves) return 0;
  static const char null_wave[FTN163_MAX_WAVE];
  while (Gap_size(inst->waves) <= waveid) {
    if (!Gap_add(inst->waves, null_wave)) return 0;
  }
  if (elSize) *elSize = Gap_elSize(inst->waves);
  return Gap_get(inst->waves, waveid);
}

FTPatRow *FTSong_get_row(FTSong *song, size_t track,
                         size_t pattern, size_t row) {
  if (!song || !song->patterns || row >= song->rows_per_pattern) return 0;
  GapList **result = Gap_get(song->patterns, track);
  if (!result) return 0;
  GapList *track_patterns = *result;
  if (Gap_size(track_patterns) <= pattern) {
    FTPatRow blankPattern[FTPAT_MAX_ROWS];
    for (size_t i = 0; i < song->rows_per_pattern; ++i) {
      blankPattern[i].note = FTNOTE_WAIT;
      blankPattern[i].instrument = FTINST_NONE;
      blankPattern[i].volume = FTVOLCOL_NONE;
      for (size_t j = 0; j < FTPAT_MAX_EFFECTS; ++j) {
        blankPattern[i].effects[j].fx = 0;
        blankPattern[i].effects[j].value = 0;
      }
    }
    do {
      Gap_add(track_patterns, blankPattern);
    } while (Gap_size(track_patterns) <= pattern);
  }
  FTPatRow *pattern_base = Gap_get(track_patterns, pattern);
  return pattern_base + row;
}
