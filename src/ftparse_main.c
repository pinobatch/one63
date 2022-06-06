#include <stdio.h>
#include <stdlib.h>
#include "ftkeywords.h"
#include "ftmodule.h"

FTModule *FTModule_fromtxt(FILE *restrict infp, const char *restrict filename);

// Dumping //////////////////////////////////////////////////////////

const char *const FT_parameter_names[] = {
  "volume", "arpeggio", "pitch", "hi-pitch", "timbre"
};

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
    if (expansion & (1 << i)) printf("Uses %s\n", FT_expansion_names[i]);
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

void FTModule_dump(FTModule *module) {
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
           FT_expansion_names[env->chipid], FT_parameter_names[env->parameter],
           env->envid, env->env_length);
    hexdump(env->env_data, env->env_length, stdout);
  }

  for (size_t i = 0; i < Gap_size(module->instruments); ++i) {
    FTPSGInstrument *inst = Gap_get(module->instruments, i);
    printf("%s instrument %zu with volume env %d, arpeggio env %d, pitch env %d, timbre %d\n",
           FT_expansion_names[inst->chipid], i, inst->envid_volume, inst->envid_arpeggio, inst->envid_pitch, inst->envid_timbre);
    if (inst->waves) {
      for (size_t w = 0; w < Gap_size(inst->waves); ++w) {
        hexdump(Gap_get(inst->waves, w), Gap_elSize(inst->waves), stdout);
      }
    }
  }

  for (size_t i = 0; i < Gap_size(module->songs); ++i) {
    FTSong *s = Gap_get(module->songs, i);

    printf("song %zu: %u rows per pattern, speed %u, tempo %u, %zu order rows\n",
           i + 1U, s->rows_per_pattern, s->start_speed, s->start_tempo,
           Gap_size(s->order));
    for (size_t r = 0; r < Gap_size(s->order); ++r) {
      const unsigned char *order_row = Gap_get(s->order, r);
      printf("order row $%02zu: ", r);
      hexdump(order_row, Gap_elSize(s->order), stdout);
    }
    for (size_t t = 0; t < Gap_size(s->patterns); ++t) {
      GapList *track_patterns = *(GapList **)Gap_get(s->patterns, t);
      printf("song %zu track %zu has %zu patterns\n",
             i + 1U, t + 1U, Gap_size(track_patterns));
      for (size_t p = 0; p < Gap_size(track_patterns); ++p) {
        FTPatRow *rows = FTSong_get_row(s, t, p, 0);
        for (size_t r = 0; r < s->rows_per_pattern; ++r) {
          printf("%02zX:%02zX", p, r);
          dump_pattern_row(&rows[r], 1);
        }
      }
    }
  }
}

// Driver program ///////////////////////////////////////////////////

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
  if (!module) {
    fprintf(stderr, "%s: error loading\n", filename);
    return EXIT_FAILURE;
  }
  // Dump parsed data
  puts("Done parsing module");
  FTModule_dump(module);
  FTModule_delete(module);

  return 0;
}
