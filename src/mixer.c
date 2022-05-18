/*

Minimalist wavetable synthesizer

Copyright 2022 Damian Yerrick

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/

/* to build:
gcc -Wall -Wextra -Os -fsanitize=undefined -o mixer mixer.c canonwav.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "canonwav.h"

// The mixer proper /////////////////////////////////////////////////

#define SIZEOF_WAVERAM 256
#define NUM_VOICES 16

typedef struct WtVoice {
  uint_fast32_t frequency;
  uint_fast32_t phase;
  uint8_t start, length, volume;
} WtVoice;

typedef struct WtMixer {
  uint8_t waveram[SIZEOF_WAVERAM];
  WtVoice voices[NUM_VOICES];
} WtMixer;

void WtMixer_mix(WtMixer *self, uint16_t *out, size_t num_samples) {
  // Clear mix buffer
  for (size_t t = 0; t < num_samples; ++t) {
    out[t] = 0;
  }

  for (size_t v = 0; v < NUM_VOICES; ++v) {
    WtVoice *voice = &self->voices[v];
    unsigned int volume = voice->volume;
    if (volume == 0) continue;

    uint_fast32_t frequency = voice->frequency;
    uint_fast32_t phase = voice->phase;
    unsigned int start = voice->start;
    unsigned int length = voice->length;

    for (size_t t = 0; t < num_samples; ++t) {
      size_t waveram_addr = ((phase >> 16) + start) % 256;
      out[t] += volume * self->waveram[waveram_addr];
      phase += frequency;
      if (phase >= length << 16) phase -= length << 16;
    }
    voice->phase = phase;
  }
}

// Wave output //////////////////////////////////////////////////////

#define OUTRATE 48000
#define SAMPLES_PER_TICK 800
#define WAVELEN 32
#define NOTE1_FREQ 246.94
#define NOTE2_FREQ 311.13
#define NOTE3_FREQ 369.99

const uint_least32_t chord_freqs[3] = {
  NOTE1_FREQ * WAVELEN * 65536 / OUTRATE,
  NOTE2_FREQ * WAVELEN * 65536 / OUTRATE,
  NOTE3_FREQ * WAVELEN * 65536 / OUTRATE,
};

int main(void) {
  WAVEWRITER *out = wavewriter_open("out.wav");
  if (!out) {
    fputs("couldn't open wave for writing\n", stderr);
    return EXIT_FAILURE;
  }
  wavewriter_setrate(out, OUTRATE);
  wavewriter_setchannels(out, 1);
  wavewriter_setdepth(out, 16);
  
  WtMixer mixer;

  // Initialize wave RAM
  memset(mixer.waveram, 0, sizeof(mixer.waveram));
  for (size_t i = 0; i < WAVELEN; ++i) {
    mixer.waveram[i] = i / 2 * 255 / (WAVELEN / 2 - 1);
  }
  if (0) {
    puts("waveram:");
    for (size_t i = 0; i < sizeof mixer.waveram / sizeof mixer.waveram[0]; ++i) {
      printf("%02x", mixer.waveram[i]);
    }
    fputc('\n', stdout);
  }

  // Initialize voices
  for (size_t v = 0; v < NUM_VOICES; ++v) {
    mixer.voices[v].volume = 0;
  }
  for (size_t v = 0; v < sizeof chord_freqs / sizeof chord_freqs[0]; ++v) {
    mixer.voices[v].frequency = chord_freqs[v];
    mixer.voices[v].phase = 0;
    mixer.voices[v].start = 0;
    mixer.voices[v].length = WAVELEN;
    printf("chord_freqs[%zu] = %u\n", v, (unsigned)chord_freqs[v]);
  }

  for (size_t tick = 0; tick < 60; ++tick) {
    uint16_t mixbuf[SAMPLES_PER_TICK];
    for (size_t v = 0; v < sizeof chord_freqs / sizeof chord_freqs[0]; ++v) {
      mixer.voices[v].volume = 60 - tick;
    }
    WtMixer_mix(&mixer, mixbuf, SAMPLES_PER_TICK);

    // Recenter
    int mixbias = 0;
    for (size_t v = 0; v < NUM_VOICES; ++v) {
      mixbias -= mixer.voices[v].volume * 128;
    }
    short outbuf[SAMPLES_PER_TICK];
    for (size_t t = 0; t < SAMPLES_PER_TICK; ++t) {
      outbuf[t] = mixbuf[t] + mixbias;
    }
    wavewriter_write(outbuf, SAMPLES_PER_TICK, out);
  }

  wavewriter_close(out);
  out = 0;
  return 0;
}
