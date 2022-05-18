wavetable project
=================

Developing a Namco 163-inspired wavetable synthesizer.  Differences:

- samples in the 256-sample wave memory can be 0 to 255 instead of
  0 to 15
- up to 16 voices, which don't take away from wave memory
- intent to support 18157 and 48000 Hz, with no multiplexer whine

Currently renders a chord to a wave file.  Nowhere near ready for
public consumption.

License: zlib
