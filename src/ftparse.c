#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "ftkeywords.h"
#include "ftmodule.h"

#define FT_MAX_CHARS_PER_CHANNEL 27
#define FT_MAX_LINE_LEN (16 + FT_MAX_CHANNELS * FT_MAX_CHARS_PER_CHANNEL)

const char WHITESPACE_CHARACTERS[] = " \t\n\v\f\r";

const char *const expansion_names[] = {
  "VRC6", "VRC7", "FDS", "MMC5", "N163", "YM2149"
};

const char *const macro_dim_names[] = {
  "volume", "arpeggio", "pitch", "hi-pitch", "timbre"
};

const char *const syntax_error_msgs[] = {
  "unknown error",
  "not enough header values (expected 5)",
  "macro dimension out of range (expected 0 through 4)",
  "expected colon after header values",
  "unknown note letter (expected A through G, ., -, or =)",
  "unknown accidental (expected -, #, or b)",
  "unknown octave (expected 0 through 7)",
  "NUL in pattern pitch",
  "NUL in pattern instrument ID",
  "unknown instrument (expected 00-FF, &&, or ..)",

  "NUL in volume", // 10
  "unknown volume (expected 0 through F or .)",
  "NUL in effect", // WITHIN effect; NUL at end of effect is end of row
  "unknown effect parameter (expected 0 through F or .)",
  "internal error: no channel pitch type",
  "noise pitch: expected hexadecimal digit",
};

const unsigned char letter_to_semitone['G' - 'A' + 1] = {
  9, 11, 0, 2, 4, 5, 7
};


/**
 * Calls strtol() up to out_count times.
 * Each long consists of zero or more whitespace characters (which
 * are discarded), an optional plus or minus sign, an optional base
 * prefix if base is 0, and a sequence of digits.
 * @param s pointer to a byte string
 * @param str_end if not NULL, pointer to the first character of s
 * from which integers were not read
 * @param outptr where to write integers read from the string
 * @param out_count maximum number of integers to read
 * @param base the numeric base, 0 or 2-36; if 0 default to 10 and
 * recognize base prefixes "0" and "0x"
 * @return number of valid integers read
 */
size_t strtol_multi(const char *restrict s, char **restrict str_end,
                    long *restrict outptr, size_t out_count, int base) {
  size_t num_read = 0;
  
  while (num_read < out_count) {
    char *this_str_end;
    long value = strtol(s, &this_str_end, base);
    if (s == this_str_end) break;  // integer was not read
    if (outptr) *outptr++ = value;
    ++num_read;
    s = this_str_end;
  }
  if (str_end) *str_end = (char *)s;
  return num_read;
}

/**
 * @param macro_header 5 bytes of header data will be written here
 * (order: dimension ID 0-4, macro ID 0-255,
 * loop point (or <0 if none), release point (or <0 if none),
 * sense of arpeggio (0: absolute; 1: fixed; 2: relative; ?: scheme))
 * @param macro_data up to 256 step values will be written here
 * @return 0 for empty macro, step count (>0) for good macro,
 * or <0 for bad macro
 */
int parse_macro(const char *restrict s,
                long *restrict macro_header, long *restrict macro_data) {
  // 5 ints (parameter, macro ID, loop point, release point, arpeggio sense)
  // then colon then macro contents
  char *str_end;
  size_t nvalues = strtol_multi(s, &str_end, macro_header, 5, 10);
  if (nvalues != 5) return -1;
  if (macro_header[0] < 0 || macro_header[0] >= 5) return -2;
  s = str_end;
  while (*s && isspace(*s)) ++s;  // eat colon
  if (*s++ != ':') return -3;
  return strtol_multi(s, &str_end, macro_data, 256, 10);
}

/**
 * Translates uppercase or lowercase hexadecimal digit characters to
 * the digit value (0-15) and non-digits to a negative value.
 */
int parsexdigit(int ch) {
  if (isdigit(ch)) return ch - '0';
  if (!isxdigit(ch)) return -1;
  return toupper(ch) - 'A' + 10;
}

#define FTCHPITCH_NORMAL 0
#define FTCHPITCH_2A03_NOISE 1

/**
 * Return a 3-character code into a wait, cut, release, or pitch
 * @param s pointer to the start of the string
 * @param str_end if not NULL, the end of the converted portion of s
 * is written here
 * @param pitch_type the channel's type of pitch (normal, 2A03 noise, etc.)
 * @return the parsed positive semitone number or FTNOTE_* value,
 * or a negative error code
 */
int parse_pitch(const char *restrict s, char **restrict str_end,
                unsigned int pitch_type) {
  if (str_end) *str_end = (char *)s;
  while (*s && isspace(*s)) ++s;
  int ch_note = *s++;
  if (!ch_note) return -7;
  int ch_accidental = *s++;
  if (!ch_accidental) return -7;
  int ch_octave = *s++;
  if (!ch_octave) return -7;
  // All channels share wait and rest
  if (ch_note == '.') {
    if (str_end) *str_end = (char *)s;
    return FTNOTE_WAIT;
  } else if (ch_note == '-') {
    if (str_end) *str_end = (char *)s;
    return FTNOTE_CUT;
  } else if (ch_note == '=') {
    if (str_end) *str_end = (char *)s;
    return FTNOTE_RELEASE;
  }
  // Handle pitches for each channel pitch type
  switch (pitch_type) {
    case FTCHPITCH_NORMAL: {
      if (ch_note < 'A' || ch_note > 'G') return -4;
      int semitone = letter_to_semitone[ch_note - 'A'];
      switch (ch_accidental) {
        case '-': break;
        case '#': semitone += 1; break;
        case 'b': semitone -= 1; break;
        default: return -5;
      }
      if (ch_octave < '0' || ch_octave > '0' + FTNOTE_MAX_OCTAVE) return -6;
      semitone += (ch_octave - '0') * 12;
      if (str_end) *str_end = (char *)s;
      return semitone;
    } break;
    case FTCHPITCH_2A03_NOISE: {
      int semitone = parsexdigit(ch_note);
      if (semitone < 0) return -15;
      if (str_end) *str_end = (char *)s;
      return semitone;
    } break;
    default: return -14;
  }
}

int parse_pattern_effects(const char *restrict s, char **restrict str_end,
                          FTPatEffect *restrict outptr, size_t out_count) {
  if (str_end) *str_end = (char *)s;
  (void) outptr;
  (void) out_count;
  return 0;
}

#define TRACK_USING_NOISE 3

/**
 * Parse pattern row data
 * @param out_count maximum number of columns to read
 * @return number of columns read if all row data good,
 * or <0 for bad row data
 */
int parse_pattern_row(const char *restrict s, char **restrict str_end,
                      FTPatRow *restrict outptr, size_t out_count) {
  size_t num_read = 0;
//  fprintf(stderr, "parse_pattern_row %s\nmax %zu columns\n", s, out_count);

  while (num_read < out_count && num_read < INT_MAX) {
    // Eat space + colon
    while (*s && isspace(*s)) ++s;
    if (!*s) break;
    if (*s++ != ':') return -3;
    char *semitone_end = 0;
    int pitch_type = (num_read == TRACK_USING_NOISE)
                     ? FTCHPITCH_2A03_NOISE : FTCHPITCH_NORMAL;
    int semitone = parse_pitch(s, &semitone_end, pitch_type);
    if (semitone < 0) return semitone;
    s = semitone_end;
    // Parse instrument
    while (*s && isspace(*s)) ++s;
    int ch_insthi = *s++, instrument;
    if (!ch_insthi) return -8;
    int ch_instlo = *s++;
    if (!ch_instlo) return -8;
    if (ch_insthi == '&') {
      instrument = FTINST_LEGATO;
    } else if (ch_insthi == '.') {
      instrument = FTINST_NONE;
    } else {
      int d_insthi = parsexdigit(ch_insthi), d_instlo = parsexdigit(ch_instlo);
      if (d_insthi < 0 || d_instlo < 0) return -9;
      instrument = d_insthi * 16 + d_instlo;
    }
    // Parse volume
    while (*s && isspace(*s)) ++s;
    int volumedigit = *s++, volume = FTVOLCOL_NONE;
    if (!volumedigit) return -10;
    if (volumedigit != '.') {
      volume = parsexdigit(volumedigit);
      if (volume < 0) return -11;
    }
    outptr[num_read].note = semitone;
    outptr[num_read].instrument = instrument;
    outptr[num_read].volume = volume;
    // Parse effects
    size_t effects_read = 0;
    while (effects_read < FTPAT_MAX_EFFECTS) {
      while (*s && isspace(*s)) ++s;
      if (!*s || *s == ':') break;

      // TODO: Flesh this out
      int ch_fx_type = *s++;
      int ch_fx_hi = *s++;
      if (!ch_fx_hi) return -12;
      int ch_fx_lo = *s++;
      if (!ch_fx_lo) return -12;
      if (ch_fx_type == '.') continue;  // No effect
      int fx_hi = parsexdigit(ch_fx_hi), fx_lo = parsexdigit(ch_fx_lo);
      if (fx_hi >= 0 && fx_lo >= 0) {
        outptr[num_read].effects[effects_read].fx = ch_fx_type;
        outptr[num_read].effects[effects_read].value = fx_hi * 16 + fx_lo;
        ++effects_read;
      }
    }
    for (; effects_read < FTPAT_MAX_EFFECTS; ++effects_read) {
      outptr[num_read].effects[effects_read].fx = 0;
      outptr[num_read].effects[effects_read].value = 0;
    }
    // Count the column
    if (str_end) *str_end = (char *)s;
    num_read += 1;
  }
  return num_read;
}

void dump_pattern_row(const FTPatRow *row, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    printf(" : %02x%02x%02x", row[i].note, row[i].instrument, row[i].volume);
    for (size_t j = 0; j < FTPAT_MAX_EFFECTS; ++j) {
      unsigned int f = row[i].effects[j].fx;
      if (f) {
        printf(" %c%02x", f, row[i].effects[j].value);
      } else {
        fputs(" ...", stdout);
      }
    }
  }
  putchar('\n');
}

void dump_expansion(unsigned int expansion) {
  if (!expansion) puts("2A03-only module");
  for (size_t i = 0; i < 6; ++i) {
    if (expansion & (1 << i)) printf("Uses %s\n", expansion_names[i]);
  }
}

void hexdump(const void *restrict start, size_t length, FILE *restrict fp) {
  size_t chars_this_line = 0;
  const unsigned char *s = start;
  for (size_t i = 0; i < length; ++i) {
    fprintf(fp, "%02x ", *s++);
    if (++chars_this_line >= 16) {
      fputc('\n', fp);
      chars_this_line = 0;
    }
  }
  if (chars_this_line) fputc('\n', fp);
}

void FTModule_delete(FTModule *module) {
  if (!module) return;
  free(module->title);
  free(module->author);
  free(module->copyright);
  Gap_delete(module->songs);
  // Delete wave RAM data if any
  if (module->instruments) {
    while (!Gap_isEmpty(module->instruments)) {
      FTPSGInstrument *ws = Gap_get(module->instruments, 0);
      Gap_delete(ws->waves);
      Gap_remove(module->instruments, 0);
    }
    Gap_delete(module->instruments);
  }
  // Delete envelope data if any
  if (module->all_envelopes) {
    while (!Gap_isEmpty(module->all_envelopes)) {
      FTEnvelope **ws = Gap_get(module->all_envelopes, 0);
      free(*ws);
      Gap_remove(module->all_envelopes, 0);
    }
    Gap_delete(module->all_envelopes);
  }
  free(module);
}

#define EXPECTED_INSTS 16
#define EXPECTED_ENVS_PER_INST 2
#define EXPECTED_SONGS 16

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

/**
 * Allocates an envelope (or sequence or macro) as a struct with a
 * flexible member.  Caller must free.  Takes longs to interoperate
 * with strtol_multi().
 * @param header a 5-tuple of (long[]){parameter, envelope ID,
 * loop point, release point, arpeggio sense}
 * @param env_data value for each tick
 * @param env_length number of ticks
 */
FTEnvelope *FTModule_pack_env(const long *restrict header,
                              const long *restrict env_data, size_t env_length) {
  if (env_length > FTENV_MAX_TICKS) return 0;
  FTEnvelope *env = malloc(sizeof(FTEnvelope) + env_length);
  if (!env) return 0;
  env->chipid = FTENVPOOL_MMC5;
  env->parameter = header[0];
  env->envid = header[1];
  env->loop_point = header[2] >= 0 ? header[2] : 255;
  env->release_point = header[3] >= 0 ? header[3] : 255;
  env->arpeggio_sense = header[4];
  env->env_length = env_length;
  for (size_t i = 0; i < env_length; ++i) env->env_data[i] = env_data[i];
  return env;
}

/**
 * Inserts blank instruments until at least inst+1 instruments
 * are present then returns Gap_get(instruments, instid).
 */
FTPSGInstrument *FTModule_get_instrument(FTModule *module, size_t instid) {
  if (!module || !module->instruments) return 0;
  static const FTPSGInstrument null_instrument =
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
  while (Gap_size(module->instruments) <= instid) {
    if (!Gap_add(module->instruments, &null_instrument)) return 0;
  }
  return Gap_get(module->instruments, instid);
}

/**
 * Inserts blank waves into instrument instid until at least waveid+1
 * waves are present then returns Gap_get(waves, waveid).
 * @param elSize if not null, the size of each wave is written here
 */
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


/**
 * @param infp a text file
 * @param filename a filename to display in error messages
 */
FTModule *FTModule_fromtxt(FILE *restrict infp, const char *restrict filename) {
  if (!filename) filename = "<input>";
  FTModule *module = FTModule_new();
  if (!module) return 0;
  char linebuf[FT_MAX_LINE_LEN];
  size_t linenum = 0;
  
  while (fgets(linebuf, sizeof linebuf, infp)) {
    linenum += 1;
    // Strip leading whitespace
    char *linepos = linebuf;
    while (*linepos && isspace(*linepos)) ++linepos;
    if (*linepos == 0 || *linepos == '#') continue;  // Skip comment
    // Find and skip keyword
    size_t keyword_len = strcspn(linepos, WHITESPACE_CHARACTERS);
    const struct FtKeyword *kw = ftkw_lookup(linepos, keyword_len);
    if (!kw) {
      printf("%s:%zu: no keyword of length %zu starting at %c\n",
             filename, linenum, keyword_len, *linepos);
      continue;
    }
    linepos += keyword_len;
    while (*linepos && isspace(*linepos)) ++linepos;

    // Dispatch
    long first_value;
    switch (kw->kwid) {
      case FTKW_NULL:
        fprintf(stderr, "%s:%zu: internal error: FTKW_NULL found\n", filename, linenum);
        break;
      case FTKW_TITLE:
      case FTKW_AUTHOR:
      case FTKW_COPYRIGHT:
      case FTKW_COMMENT:  // this'll be tricky because multiline
      case FTKW_SPLIT:
        break;  // Ignore metadata for now
      case FTKW_VIBRATO:
        break;  // Player does not handle very old legacy modules
      case FTKW_COLUMNS:
        break;  // Per-song; internal representation always uses 4 effect columns
      case FTKW_MACHINE: {
        char *str_end;
        first_value = strtol(linepos, &str_end, 0);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: machine class is blank\n",
                  filename, linenum);
          break;
        }
        if (first_value < 0 || first_value > 1) {
          fprintf(stderr, "%s:%zu: unexpected machine class %ld\n",
                  filename, linenum, first_value);
          break;
        }
        module->tvSystem = first_value;
      } break;
      case FTKW_FRAMERATE: {
        char *str_end;
        first_value = strtol(linepos, &str_end, 0);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: update rate is blank\n", filename, linenum);
          break;
        }
        if (first_value < 0 || first_value > 800) {
          fprintf(stderr, "%s:%zu: update rate %ld out of range\n",
                  filename, linenum, first_value);
          break;
        }
        module->tickRate = first_value;
      } break;
      case FTKW_EXPANSION: {
        char *str_end;
        first_value = strtol(linepos, &str_end, 0);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: expansion flags is blank\n", filename, linenum);
          break;
        }
        module->expansion = first_value;
      } break;
      case FTKW_N163CHANNELS: {
        char *str_end;
        first_value = strtol(linepos, &str_end, 0);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: Namco 163 channel count is blank\n",
                  filename, linenum);
          break;
        }
        if (first_value < 1 || first_value > 8) {
          fprintf(stderr, "%s:%zu: Namco 163 channel count %ld out of range (expected 1 to 8)\n",
                  filename, linenum, first_value);
          break;
        }
        module->wsgNumChannels = first_value;
      } break;
      case FTKW_MACRO: {
        long macro_header[5];
        long macro_data[256];
        int nvalues = parse_macro(linepos, macro_header, macro_data);
        if (nvalues < 0) {
          size_t msgid = nvalues >= -3 ? -nvalues : 0;
          fprintf(stderr, "%s:%zu: %s: %s\n",
                  filename, linenum, kw->name, syntax_error_msgs[msgid]);
          break;
        }
        FTEnvelope *macro = FTModule_pack_env(macro_header, macro_data, nvalues);
        if (!macro || !Gap_add(module->all_envelopes, &macro)) {
          fprintf(stderr, "%s:%zu: %s: out of memory\n", filename, linenum, kw->name);
          break;
        }
      } break;
      case FTKW_MACRON163: {
        long macro_header[5];
        long macro_data[256];
        int nvalues = parse_macro(linepos, macro_header, macro_data);
        if (nvalues < 0) {
          size_t msgid = nvalues >= -3 ? -nvalues : 0;
          fprintf(stderr, "%s:%zu: %s: %s\n",
                  filename, linenum, kw->name, syntax_error_msgs[msgid]);
          break;
        }
        FTEnvelope *macro = FTModule_pack_env(macro_header, macro_data, nvalues);
        if (macro) macro->chipid = FTENVPOOL_N163;
        if (!macro || !Gap_add(module->all_envelopes, &macro)) {
          fprintf(stderr, "%s:%zu: %s: out of memory\n", filename, linenum, kw->name);
          break;
        }
      } break;
      case FTKW_INST2A03: {
        // 6 ints (instrument ID, macro ID for each dimension)
        // then name
        long params[6];
        char *str_end;
        size_t nvalues = strtol_multi(linepos, &str_end, params, 6, 10);
        if (nvalues != 6) {
          fprintf(stderr, "%s:%zu: %s: expected 6 params\n", filename, linenum, kw->name);
          break;
        }
        FTPSGInstrument *inst = FTModule_get_instrument(module, params[0]);
        if (!inst) {
          fprintf(stderr, "%s:%zu: %s: out of memory for instrument %ld\n", filename, linenum, kw->name, params[0]);
        }
        inst->chipid = FTENVPOOL_2A03;
        inst->envid_volume = params[1];
        inst->envid_arpeggio = params[2];
        inst->envid_pitch = params[3];
        inst->envid_timbre = params[5];
      } break;
      case FTKW_INSTN163: {
        // 9 ints (instrument ID, macro ID for each dimension,
        // wave length, wave RAM start address, wave count) then name
        long params[9];
        char *str_end;
        size_t nvalues = strtol_multi(linepos, &str_end, params, 9, 10);
        if (nvalues != 9) {
          fprintf(stderr, "%s:%zu: %s: expected 9 params\n", filename, linenum, kw->name);
          break;
        }

        FTPSGInstrument *inst = FTModule_get_instrument(module, params[0]);
        if (!inst) {
          fprintf(stderr, "%s:%zu: %s: out of memory for instrument %ld\n", filename, linenum, kw->name, params[0]);
        }
        inst->chipid = FTENVPOOL_N163;
        inst->envid_volume = params[1];
        inst->envid_arpeggio = params[2];
        inst->envid_pitch = params[3];
        inst->envid_timbre = params[5];
        inst->waveram_length = params[6];
        inst->waveram_address = params[7];
        inst->waves = Gap_new(inst->waveram_length, params[8]);
        if (!inst) {
          fprintf(stderr, "%s:%zu: %s: out of memory for instrument %ld's waves\n", filename, linenum, kw->name, params[0]);
        }
      } break;
      case FTKW_N163WAVE: {
        // 2 ints (instrument ID, timbre value)
        // then colon then samples 0-15
        long wave_header[2];
        long wave_data[240];
        char *str_end;
        size_t nvalues = strtol_multi(linepos, &str_end, wave_header, 2, 10);
        if (nvalues != 2) {
          fprintf(stderr, "%s:%zu: %s: expected 2 params\n", filename, linenum, kw->name);
          break;
        }
        linepos = str_end;
        while (*linepos && isspace(*linepos)) ++linepos;  // eat colon
        if (*linepos++ != ':') {
          fprintf(stderr, "%s:%zu: missing colon after M163 params\n", filename, linenum);
          break;
        }
        nvalues = strtol_multi(linepos, &str_end, wave_data, 240, 10);
        if (nvalues < 4 || nvalues >= 240) {
          fprintf(stderr, "%s:%zu: N163 wave has %ld steps (expected 2 to 240)\n", filename, linenum, nvalues);
          break;
        }

        size_t max_nvalues = 0;
        unsigned char *wave = FTModule_get_wave(module, wave_header[0], wave_header[1], &max_nvalues);
        if (!wave) {
          fprintf(stderr, "%s:%zu: %s: out of memory for instrument %ld wave %ld\n", filename, linenum, kw->name, wave_header[0], wave_header[1]);
        }
        for (size_t i = 0; i < nvalues && i < max_nvalues; ++i) {
          wave[i] = wave_data[i];
        }
      } break;

      // These are stateful
      // TRACK, COLUMNS, ORDER, and ROW affect the most recent TRACK
      // ROW affects the current PATTERN of the most recent TRACK
      case FTKW_TRACK: {
        // 3 ints (rows per pattern, starting speed, starting tempo) then title
        long track_header[3];
        char *str_end;
        size_t nvalues = strtol_multi(linepos, &str_end, track_header, 3, 10);
        if (nvalues != 3) {
          fprintf(stderr, "%s:%zu: %s: expected 3 params\n", filename, linenum, kw->name);
          break;
        }
        printf("Add song, %ld rows per pattern, speed %ld, tempo %ld\n",
               track_header[0], track_header[1], track_header[2]);
      } break;
      case FTKW_ORDER: {
        // hypermeasure id then colon then as many as there are rows in all chips
        long pattern_ids[FT_MAX_CHANNELS];
        char *str_end;
        first_value = strtol(linepos, &str_end, 16);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: no order ID\n", filename, linenum);
          break;
        }
        linepos = str_end;
        while (*linepos && isspace(*linepos)) ++linepos;  // eat colon
        if (*linepos++ != ':') {
          fprintf(stderr, "%s:%zu: missing colon after params\n", filename, linenum);
          break;
        }

        size_t ncols = strtol_multi(linepos, &str_end, pattern_ids, FT_MAX_CHANNELS, 16);
        if (ncols < FT_MIN_CHANNELS || ncols > FT_MAX_CHANNELS) {
          fprintf(stderr, "%s:%zu: order row length out of range\n", filename, linenum);
          break;
        }
        printf("order row $%02lx:", first_value);
        for (size_t i = 0; i < ncols; ++i) {
          printf(" %02lx", pattern_ids[i]);
        }
        putchar('\n');
      } break;
      case FTKW_PATTERN: {
        char *str_end;
        first_value = strtol(linepos, &str_end, 16);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: no pattern ID\n",
                  filename, linenum);
          break;
        }
        printf("Writing rows to last track's pattern 0x%02lx\n", first_value);
      } break;
      case FTKW_ROW: {
        // hex row ID, then colon, then colon-separated row contents
        char *str_end;
        first_value = strtol(linepos, &str_end, 16);
        if (linepos == str_end) {
          fprintf(stderr, "%s:%zu: no pattern ID\n", filename, linenum);
          break;
        }
        linepos = str_end;
        
        FTPatRow row[FT_MAX_CHANNELS];
        int nvalues = parse_pattern_row(linepos, &str_end,
                                        row, sizeof row/sizeof row[0]);
        printf("pattern row %02lx has %d columns\n", first_value, nvalues);
        dump_pattern_row(row, nvalues);
      } break;
    }
  }
  // Dump parsed data
  puts("Done parsing module");
  puts(module->tvSystem ? "For 2A07 (PAL NES)" : "For 2A03 (NTSC NES)");
  if (module->tickRate) {
    printf("Update rate is %u Hz\n", module->tickRate);
  } else {
    puts("Update rate is default for machine");
  }
  dump_expansion(module->expansion);
  if (module->expansion & (1 << FTENVPOOL_N163)) {
    // though all 8 are coded in the ORDER and PATTERNs,
    // only this many are sent to the WSG
    printf("First %u of 8 N163 channels are used\n", module->wsgNumChannels);
  }

  for (size_t i = 0; i < Gap_size(module->all_envelopes); ++i) {
    FTEnvelope **result = Gap_get(module->all_envelopes, i);
    if (!result || !*result) {
      fprintf(stderr, "ouch! all_envelopes[%zu] is null\n", i);
      continue;
    }
    FTEnvelope *env = *result;
    printf("chip %s %s macro %d with %d steps\n",
           expansion_names[env->chipid], macro_dim_names[env->parameter],
           env->envid, env->env_length);
    hexdump(env->env_data, env->env_length, stdout);
  }

  for (size_t i = 0; i < Gap_size(module->instruments); ++i) {
    FTPSGInstrument *inst = Gap_get(module->instruments, i);
    printf("%s instrument %zu with volume env %d, arpeggio env %d, pitch env %d, timbre %d\n",
           expansion_names[inst->chipid], i, inst->envid_volume, inst->envid_arpeggio, inst->envid_pitch, inst->envid_timbre);
    if (inst->waves) {
      for (size_t w = 0; w < Gap_size(inst->waves); ++w) {
        hexdump(Gap_get(inst->waves, w), Gap_elSize(inst->waves), stdout);
      }
    }
  }


  return module;
}

int main(void) {
  const char *filename = "parsertest.txt";
//  const char *filename = "audio-private/draft.txt";

  FILE *infp = fopen(filename, "r");
  if (!infp) {
    perror(filename);
    return EXIT_FAILURE;
  }
  FTModule *module = FTModule_fromtxt(infp, filename);
  fclose(infp);
  FTModule_delete(module);

  return 0;
}
