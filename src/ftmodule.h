#ifndef FTMODULE_H
#define FTMODULE_H
#include "gaplist.h"

// Instruments //////////////////////////////////////////////////////

#define FTENV_MAX_TICKS 255
#define FTN163_MAX_WAVE 240

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

extern const char *const FT_expansion_names[FT_NUM_ENVPOOLS];
extern const unsigned char FT_expansion_channels[FT_NUM_ENVPOOLS];

typedef struct {
  unsigned char chipid;  // chipid and envid match those in FTEnvelope
  // each may be set to UCHAR_MAX meaning none assigned
  unsigned char envid_volume;
  unsigned char envid_arpeggio;
  unsigned char envid_pitch;
  // not supporting hi-pitch because arpeggio is more stable
  unsigned char padding0;
  unsigned char envid_timbre;

  // ancillary data for each instrument
  // for N163, waves is a GapList<char[waveram_length]>
  unsigned char waveram_length;
  unsigned char waveram_address;
  GapList *waves;
} FTPSGInstrument;

typedef struct {
  unsigned char chipid, parameter, envid;  // these form the key
  unsigned char padding0;
  unsigned char loop_point, release_point, arpeggio_sense, env_length;
  unsigned char env_data[];
} FTEnvelope;

// Patterns /////////////////////////////////////////////////////////

#define FTSONG_MAX_PATTERNS 256
#define FTPAT_MAX_ROWS 256
#define FTPAT_MAX_EFFECTS 4
#define FT_2A03_NUM_CHANNELS 5
#define FT_MIN_CHANNELS FT_2A03_NUM_CHANNELS
#define FT_MAX_CHANNELS 28
#define FTNOTE_MAX_OCTAVE 7
#define FT_NOISE_CHANNEL 3

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
  char *title;  // owned; realloc() on change
  GapList *order;  // GapList<unsigned char[nchannels]> order[row][track]
  GapList *patterns;  // GapList<GapList<FTPatRow[rpp]>> patterns[track][patid][row]
  unsigned short rows_per_pattern;
  unsigned char start_speed;
  unsigned char start_tempo;
} FTSong;

typedef struct {
  char *title, *author, *copyright;  // owned; realloc() on change
  unsigned char tvSystem;
  unsigned char expansion;
  unsigned char wsgNumChannels;  // channels after this are muted
  unsigned char padding1;
  unsigned int tickRate;
  // to be restored once I remember what FTTrackDef was
  // perhaps once I get to synthesis
//  GapList *instcodes;  // GapList<FTTrackDef> instcodes[track]
  GapList *instruments;  // GapList<FTPSGInstrument> psg_instruments[instid]
  GapList *all_envelopes;  // GapList<FTEnvelope *> all_envelopes[dedupeid]
  GapList *songs;  // GapList<FTSong> songs[songid];
  // TODO: come up with a solution for envelope lookup
} FTModule;

#endif
