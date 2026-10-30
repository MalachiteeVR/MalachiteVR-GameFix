# MalachiteVR GameFix

A Windows gaming diagnostics and troubleshooting utility built with **Qt 6 + C++20**.

## Current status

**v0.2.0** adds a live, read-only performance dashboard. GameFix now samples CPU usage, GPU utilization, system RAM, dedicated GPU memory, and NVIDIA GPU temperature when the NVIDIA driver exposes NVML. It also provides a heuristic bottleneck explanation based on the current readings.

## Current diagnostics

- CPU, GPU, and RAM information
- Live CPU utilization
- Live GPU utilization using Windows Performance Counters
- Live system memory usage
- Dedicated GPU memory usage through DXGI
- NVIDIA GPU temperature when NVML is available
- Basic CPU/GPU/RAM/VRAM bottleneck analysis

## Planned diagnostics

- Storage activity
- Network latency, jitter, and packet loss
- Running game detection
- Per-game performance analysis
- Safe, user-approved Windows gaming fixes
- Reversible configuration changes

GameFix will focus on explaining **why** a game is performing poorly instead of applying opaque "FPS boost" tweaks.

## Build

Requirements:

- Windows 10/11
- Qt 6 with the Widgets module
- CMake 3.21 or newer
- C++20 compiler

For the Visual Studio/MSVC build, configure CMake with the Qt MSVC kit available on your machine, for example:

```powershell
cmake -S . -B build -DQt6_DIR="C:\Qt\6.12.0\msvc2022_64\lib\cmake\Qt6"
cmake --build build --config Release
```

The executable is produced as `build\Release\malachite-gamefix.exe` with a Visual Studio generator. Use Qt's `windeployqt.exe` to copy the Qt runtime files beside the executable for local deployment.

## Project goals

- Native Windows application
- No telemetry sent to external services
- No account required
- No unsafe registry or "magic optimizer" tweaks
- Every system-changing fix should be explained and user-approved
- Keep the implementation small and maintainable
