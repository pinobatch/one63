#ifndef FTMODULE_H
#define FTMODULE_H

#define FTPAT_MAX_EFFECTS 4

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

#endif
