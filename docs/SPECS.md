# Technical Specification

- **Base Resolution:** 480 × 270
- **Resolution Multiplier:** 4×
- **Audio:** All Opus files must use stereo audio at a 48,000 Hz sample rate and CELT-only mode.

## Rendering

The engine does not clear the renderer each frame, and the output is always 16:9 (1080p). The letterbox bars never appear, so the clear is not necessary.
