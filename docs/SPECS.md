# Technical Specification

- **Base Resolution:** 480 × 270
- **Resolution Multiplier:** 4×
- **Audio:** All Opus files must use stereo audio at a 48,000 Hz sample rate and CELT-only mode, for example `ffmpeg -i input.wav -c:a libopus -application lowdelay output.opus`.
