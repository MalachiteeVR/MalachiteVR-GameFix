# MalachiteVR GameFix

A Windows gaming diagnostics and troubleshooting utility built with **Qt 6 + C++20**.

## Current status

**v0.3.0** adds the first full diagnostic pass on top of the live performance dashboard. The dashboard is responsive and can scroll when the window is resized smaller, so diagnostic text is not clipped.

## Current diagnostics

- CPU, GPU, and RAM information
- Live CPU utilization
- Live GPU utilization using Windows Performance Counters
- Live system memory usage
- Dedicated GPU memory usage through DXGI
- NVIDIA GPU temperature when NVML is available
- CPU/GPU/RAM/VRAM bottleneck analysis
- Running game detection for common PC/VR games
- Internet latency testing against 1.1.1.1
- Packet-loss measurement and latency variation reporting
- Fixed-drive free-space inspection
- Current physical-disk activity sampling
- One-shot full diagnostic report with warnings and recommendations
- Resizable, scrollable dashboard and report viewer

The full diagnostic is read-only. It does not modify Windows settings, registry values, firewall rules, power plans, or game files.

## Planned next steps

- Per-game performance history and capture
- More robust game identification
- CPU temperature and per-core utilization where reliable APIs are available
- Network adapter/link diagnostics
- Safe, user-approved Windows gaming fixes
- Reversible configuration changes

GameFix focuses on explaining **why** a game is performing poorly instead of applying opaque "FPS boost" tweaks.

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
