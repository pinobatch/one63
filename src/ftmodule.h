#ifndef FTMODULE_H
#define FTMODULE_H
#include "gaplist.h"

// Instruments //////////////////////////////////////////////////////

enum FTEnvPoolID {
  FTENVPOOL_VRC6   = 0,
  FTENVPOOL_VRC7   = 1,
  FTENVPOOL_FDS    = 2,
  FTENVPOOL_MMC5   = 3,
  FTENVPOOL_N163   = 4,
  FTENVPOOL_YM2149 = 5,
  FT_NUM_ENVPOOLS  = 6,
  FTENVPOOL_2A03 = FTENVPOOL_MMC5
};

enum FTEnvPoolID {
  FTENVPOOL_VRC6   = 0,
  FTENVPOOL_VRC7   = 1,
  FTENVPOOL_FDS    = 2,
  FTENVPOOL_MMC5   = 3,
  FTENVPOOL_N163   = 4,
  FTENVPOOL_YM2149 = 5,
  FT_NUM_ENVPOOLS  = 6,
  FTENVPOOL_2A03 = FTENVPOOL_MMC5
};

typedef struct {
  // each may be set to UCHAR_MAX meaning none assigned
  unsigned char envid_volume;
  unsigned char envid_arpeggio;
  unsigned char envid_pitch;
  unsigned char envid_timbre;
  // we are not supporting hi-pitch because arpeggio is more stable
} FTPSGInstrument;

// Patterns /////////////////////////////////////////////////////////

#define FTPAT_MAX_EFFECTS 4
#define FT_MIN_CHANNELS 5
#define FT_MAX_CHANNELS 28
#define FTNOTE_MAX_OCTAVE 7

typedef struct {
  // fx is an ascii character or NUL for none assigned
  unsigned char fx, value;
} FTPatEffect;

#define FTNOTE_RELEASE 126
#define FTNOTE_CUT 127
#define FTNOTE_WAIT 255
#define FTINST_LEGATO 254
#define FTINST_NONE 255
#define FTVOLCOL_MAX 15
#define FTVOLCOL_NONE 255

typedef struct {
  // Note 0-95: C-0 through B-7; 126: release; 127: cut; 255: hold
  // Instrument 0-127: instrument 00-7F; 254: &&; 255: unspecified
  // Volume 0-15: 0-F; 255: unchanged
  // each may be set to UCHAR_MAX for none assigned
  unsigned char note, instrument, volume, padding0;
  FTPatEffect effects[FTPAT_MAX_EFFECTS];
} FTPatRow;

// Top level ////////////////////////////////////////////////////////

typedef struct {
  const char *title;  // realloc() on change
  GapList *order;  // GapList<GapList<uint8_t>> order[row][track]
  GapList *patterns;  // GapList<GapList<FTPatRow>> patterns[track][orderid]
  unsigned char start_tempo;
  unsigned char start_speed;
  unsigned short rows_per_pattern;
} FTSong;

typedef struct {
  const char *title, *author, *copyright;  // realloc() on change
  GapList *instcodes;  // GapList<FTTrackDef> instcodes[track]
  GapList *psg_instruments[FT_NUM_ENVPOOLS];  // GapList<FTPSGInstrument> psg_instruments[poolid];
  GapList *songs;
// come up with a solution for  GapList *psg_envelopes
} FTModule;

#endif
