# MalachiteVR GameFix

A Windows gaming diagnostics and troubleshooting utility built with **Qt 6 + C++20**.

## Current status

GameFix is in early development. The first milestone is a native Windows dashboard that collects basic system information and provides the foundation for game diagnostics.

## Planned diagnostics

- CPU, GPU, and RAM information
- Hardware utilization and temperatures
- VRAM and system memory usage
- Storage activity
- Network latency, jitter, and packet loss
- Running game detection
- Performance bottleneck detection
- Safe, user-approved Windows gaming fixes
- Reversible configuration changes

GameFix will focus on explaining **why** a game is performing poorly instead of applying opaque "FPS boost" tweaks.

## Build

Requirements:

- Windows 10/11
- Qt 6 with the Widgets module
- CMake 3.21 or newer
- C++20 compiler

Build with CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable is produced as `build\Release\malachite-gamefix.exe` with a Visual Studio generator.

## Project goals

- Native Windows application
- No telemetry by default
- No account required
- No unsafe registry or "magic optimizer" tweaks
- Every system-changing fix should be explained and user-approved
- Keep the initial implementation small and maintainable
