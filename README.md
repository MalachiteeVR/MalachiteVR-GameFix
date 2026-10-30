# MalachiteVR GameFix

A Windows gaming diagnostics and troubleshooting utility built with **Qt 6 + C++20**.

## Current status

**v0.4.0** now contains the rest of the core diagnostic workflow that was planned for the first major version: persistent per-game capture/history, stronger game identification, per-core CPU telemetry, CPU/thermal-zone reporting, network adapter/link diagnostics, report export, and opt-in reversible Windows gaming fixes.

## Diagnostics and telemetry

- CPU, GPU, RAM, and operating-system information
- Live CPU utilization
- Live per-core CPU utilization using Windows Performance Counters
- Live GPU utilization using Windows Performance Counters
- Live system memory usage
- Dedicated GPU memory usage through DXGI
- NVIDIA GPU temperature when NVML is available
- CPU/thermal-zone temperature in full diagnostics when Windows ACPI exposes it
- CPU/GPU/RAM/VRAM bottleneck analysis
- Detection of common PC/VR games using process names, executable paths, and window titles
- Detected game executable path for per-game fixes
- Default gateway latency testing
- Internet latency testing against 1.1.1.1
- Packet-loss measurement and latency variation reporting
- Network adapter state, IP addresses, gateway, and RX/TX link speed
- Fixed-drive free-space inspection
- Current physical-disk activity sampling
- One-shot full diagnostic report with warnings and recommendations
- Saveable text diagnostic reports
- Resizable, scrollable dashboard and report viewer

## Performance capture and history

- One-click per-game performance capture
- One sample per second while capture is active
- Persistent local JSON history under the Windows application-data directory
- Up to 7,200 samples retained per game
- History viewer with averages for CPU, GPU, RAM, VRAM, and GPU temperature
- CSV export for captured performance data
- Clear individual game histories from the UI

## Safe gaming fixes

All fixes are opt-in and require an explicit confirmation before changing anything. GameFix stores the previous value where practical and exposes a restore action.

- Enable Windows Game Mode
- Activate the Windows High Performance power plan while remembering the previous plan
- Disable Fullscreen Optimizations for the currently detected game
- Enable or disable Hardware-Accelerated GPU Scheduling when GameFix is running elevated
- Restore saved GameFix-managed settings

HAGS changes require administrator privileges and a Windows restart. Per-game fullscreen compatibility changes require relaunching the game.

GameFix deliberately does **not** apply registry tweaks, power-plan changes, firewall changes, process kills, overclocking, or game-file modifications automatically.

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
- Explain the bottleneck before suggesting a change
- Every system-changing fix should be user-approved and reversible where practical
- Keep the implementation small, transparent, and maintainable
