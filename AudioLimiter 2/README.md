# Audio Limiter (VST3 + Standalone)

Lookahead peak limiter built with JUCE. Parameters: Threshold, Ceiling, Release, Makeup Gain, Bypass.

## Build

Requirements: CMake 3.22+, a C++17 compiler, and git (CMake downloads JUCE 8.0.4 on first configure).

- Windows: Visual Studio 2022 (Desktop C++ workload)
- macOS: Xcode command line tools
- Linux: `sudo apt install build-essential cmake git libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl1-mesa-dev`

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3 is copied to your system VST3 folder after the build. The raw output is under
`build/AudioLimiter_artefacts/Release/VST3/` and a standalone app is in `.../Standalone/`.
Rescan plugins in your DAW.

## How it works

`Source/LimiterCore.h` has no JUCE dependency. Each sample, the required gain is computed,
the minimum over a 5 ms lookahead window is taken, release smoothing is applied, and the
result is averaged over the window. Gain starts falling before the peak arrives, so the
output stays at or under the limit with no clipping.

- Latency is 5 ms (reported to the host for compensation, also while bypassed).
- Effective limit level = min(Threshold, Ceiling - Makeup), so makeup gain never pushes the signal into a hard clip.
- Ceiling also acts as a final safety clamp.

## Tests

```
g++ -std=c++17 -O2 -o test_limiter Tests/test_limiter.cpp && ./test_limiter
```
