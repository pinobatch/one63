#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "ftkeywords.h"

#define FT_MAX_CHANNELS 28
#define FT_MAX_CHARS_PER_CHANNEL 27
#define FT_MAX_LINE_LEN (16 + FT_MAX_CHANNELS * FT_MAX_CHARS_PER_CHANNEL)

const char WHITESPACE_CHARACTERS[] = " \t\n\v\f\r";

const char *const expansion_names[] = {
  "VRC6", "VRC7", "FDS", "MMC5", "N163", "YM2149"
};

const char *const macro_dim_names[] = {
  "volume", "arpeggio", "pitch", "hi-pitch", "timbre"
};

const char *const macro_error_msgs[] = {
  "unknown error",
  "not enough header values (expected 5)",
  "dimension out of range (expected 0-4)",
  "expected colon after header values",
};


/**
 * Calls strtol() up to out_count times.
 * Each long consists of zero or more whitespace characters (which
 * are discarded), 
 * an optional plus or minus sign, an optional base prefix if base
 * is 0, and a sequence of digits.
 * @param str pointer to a byte string
 * @param str_end if not NULL, pointer to the first character of str
 * from which integers were not read
 * @param outptr where to write integers read from the string
 * @param out_count maximum number of integers to read
 * @param base the numeric base, 0 or 2-36; if 0 default to 10 and
 * recognize base prefixes "0" and "0x"
 * @return number of valid integers read
 */
size_t strtol_multi(const char *restrict str, char **restrict str_end,
                    long *restrict outptr, size_t out_count, int base) {
  size_t num_read = 0;
  
  while (num_read < out_count) {
    char *this_str_end;
    long value = strtol(str, &this_str_end, base);
    if (str == this_str_end) break;  // integer was not read
    if (outptr) *outptr++ = value;
    ++num_read;
    str = this_str_end;
  }
  if (str_end) *str_end = (char *)str;
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
int parse_macro(const char *s, long *macro_header, long *macro_data) {
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

int main(void) {
  const char *filename = "parsertest.txt";
  char linebuf[FT_MAX_LINE_LEN];
  size_t linenum = 0;
  
  FILE *infp = fopen(filename, "r");
  if (!infp) {
    perror(filename);
    return EXIT_FAILURE;
  }
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
      case FTKW_COMMENT:
      case FTKW_SPLIT:
        break;  // Ignore metadata for now
      case FTKW_VIBRATO:
        break;  // Player does not handle very old legacy modules
      case FTKW_COLUMNS:
        break;  // Per-song; internal representation always uses 4 effect columns
      case FTKW_MACHINE:
        first_value = strtol(linepos, NULL, 0);
        if (first_value < 0 || first_value > 1) {
          fprintf(stderr, "%s:%zu: unexpected machine class %ld\n",
                  filename, linenum, first_value);
          break;
        }
        puts(first_value ? "For 2A07 (PAL NES)" : "For 2A03 (NTSC NES)");
        break;
      case FTKW_FRAMERATE:
        first_value = strtol(linepos, NULL, 0);
        if (first_value < 0 || first_value > 400) {
          fprintf(stderr, "%s:%zu: update rate %ld out of range\n",
                  filename, linenum, first_value);
          break;
        }
        if (first_value) {
          printf("Update rate is %ld Hz\n", first_value);
        } else {
          puts("Update rate is default for machine");
        }
        break;
      case FTKW_EXPANSION:
        first_value = strtol(linepos, NULL, 0);
        for (size_t i = 0; i < 6; ++i) {
          if (first_value & (1 << i)) printf("Uses %s\n", expansion_names[i]);
        }
        break;
      case FTKW_N163CHANNELS:
        first_value = strtol(linepos, NULL, 0);
        printf("First %ld of 8 N163 channels are used\n", first_value);
        // even though all 8 are coded in the ORDER and PATTERNs
        break;
      case FTKW_MACRO: {
        long macro_header[5];
        long macro_data[256];
        first_value = parse_macro(linepos, macro_header, macro_data);
        if (first_value < 0) {
          size_t msgid = first_value >= -3 ? -first_value : 0;
          fprintf(stderr, "%s:%zu: %s: %s\n",
                  filename, linenum, kw->name, macro_error_msgs[msgid]);
          break;
        }
        printf("2A03 %s macro %ld macro with %ld steps\n",
               macro_dim_names[macro_header[0]], macro_header[1], first_value);
      } break;
      case FTKW_MACRON163: {
        long macro_header[5];
        long macro_data[256];
        first_value = parse_macro(linepos, macro_header, macro_data);
        if (first_value < 0) {
          size_t msgid = first_value >= -3 ? -first_value : 0;
          fprintf(stderr, "%s:%zu: %s: %s\n",
                  filename, linenum, kw->name, macro_error_msgs[msgid]);
          break;
        }
        printf("N163 %s macro %ld with %ld steps\n",
               macro_dim_names[macro_header[0]], macro_header[1], first_value);
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
        fprintf(stderr, "2A03 instrument %02lx with volume %ld, arpeggio %ld, pitch %ld, hi-pitch %ld, timbre %ld\n",
                params[0], params[1], params[2], params[3], params[4], params[5]);
      } break;
      case FTKW_INSTN163: {
        // 9 ints (instrument ID, macro ID for each dimension, wave length, unk 0, unk 1)
        // then name
        long params[9];
        char *str_end;
        size_t nvalues = strtol_multi(linepos, &str_end, params, 9, 10);
        if (nvalues != 9) {
          fprintf(stderr, "%s:%zu: %s: expected 9 params\n", filename, linenum, kw->name);
          break;
        }
        fprintf(stderr, "N163 instrument %02lx with volume %ld, arpeggio %ld, pitch %ld, hi-pitch %ld, timbre %ld, %ld waves of length %ld at address %ld\n",
                params[0], params[1], params[2], params[3], params[4], params[5],
                params[8], params[6], params[7]);
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
        nvalues = strtol_multi(linepos, &str_end, wave_data, 2, 10);
        printf("N163 instrument %ld wave %ld with %ld steps\n",
               wave_header[0], wave_header[1], nvalues);
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
      case FTKW_ORDER:
        // hypermeasure id then colon then as many as there are rows in all chips
        fprintf(stderr, "%s:%zu: %s not yet handled\n", filename, linenum, kw->name);
        break;
      case FTKW_PATTERN:
        first_value = strtol(linepos, NULL, 16);
        fprintf(stderr, "%s:%zu: will write rows to pattern %02lx\n", filename, linenum, first_value);
        break;
      case FTKW_ROW:
        // hex row ID, then colon, then colon-separated row contents
        fprintf(stderr, "%s:%zu: %s not yet handled\n", filename, linenum, kw->name);
        break;
    }
  }

  fclose(infp);
  return 0;
}
