# PitchForge v2.0.0

Original real-time vocal pitch correction plugin for Windows/FL Studio.

## Engine
- Windowed autocorrelation pitch detector with parabolic period refinement.
- Confidence gating and smoothing for monophonic vocal material.
- Chromatic, major and minor target-note quantization.
- Configurable key, retune speed, correction amount, humanize and correction range.
- Real-time granular overlap/crossfade pitch shifter.
- Dry/wet mix and VST3 + standalone targets.

This is an original implementation and does not copy proprietary MetaTune code, assets, or UI.

## Build
Requires CMake 3.22+, a C++17 compiler, and internet access on the first configure so CMake can fetch JUCE 8.0.8.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The VST3 can be installed/copied to `C:\Program Files\Common Files\VST3`. FL Studio supports VST3 on Windows. Then use **Options > Manage Plugins > Find installed plugins**.
