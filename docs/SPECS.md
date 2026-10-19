# Technical Specification

- **Base Resolution:** 480 × 270
- **Resolution Multiplier:** 4×
- **Audio:** All Opus files must use stereo audio at a 48,000 Hz sample rate and CELT-only mode, for example `ffmpeg -i input.wav -c:a libopus -application lowdelay output.opus`.

## Rendering

The engine does not clear the renderer each frame, and the output is always 16:9 (1080p). The letterbox bars never appear, so the clear is not necessary.

Risk: every scene must paint an opaque background over the full logical viewport (480 × 270). If a scene does not cover the full viewport, the previous frame stays visible.

If the output aspect ratio changes, add these two lines to `engine::loop()` between `_director.update(delta)` and `_director.draw()` to paint the letterbox bars:

```cpp
SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
SDL_RenderClear(renderer);
```
