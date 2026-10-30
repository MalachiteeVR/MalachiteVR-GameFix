#include "Diagnostic.h"

#include "SystemInfo.h"
#include "Telemetry.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <pdh.h>
#include <tlhelp32.h>
#endif

#include <QStringList>

#include <algorithm>

#ifdef Q_OS_WIN
namespace
{
QString processNameFromPid(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};

    wchar_t path[32768]{};
    DWORD length = 32768;
    QString result;
    if (QueryFullProcessImageNameW(process, 0, path, &length))
        result = QString::fromWCharArray(path, static_cast<int>(length));
    CloseHandle(process);

    const int slash = result.lastIndexOf(QChar('\\'));
    return slash >= 0 ? result.mid(slash + 1) : result;
}

QString detectRunningGame()
{
    static const QStringList knownGames = {
        "vrchat.exe",
        "beatsaber.exe",
        "minecraft.exe",
        "minecraftlauncher.exe",
        "javaw.exe",
        "java.exe",
        "eldenring.exe",
        "fortniteclient-win64-shipping.exe",
        "cs2.exe",
        "csgo.exe",
        "valorant-win64-shipping.exe",
        "helldivers2.exe",
        "palworld-win64-shipping.exe",
        "r5apex.exe",
        "starfield.exe",
        "cyberpunk2077.exe",
        "gta5.exe",
        "rocketleague.exe",
        "overwatch.exe"
    };

    const HWND foreground = GetForegroundWindow();
    if (foreground) {
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        const QString foregroundProcess = processNameFromPid(pid).toLower();
        if (knownGames.contains(foregroundProcess))
            return foregroundProcess;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return "No recognized game process found";

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    QString result;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
            if (knownGames.contains(name)) {
                result = name;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    return result.isEmpty() ? "No recognized game process found" : result;
}

QString networkCheck(QStringList &warnings)
{
    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE) {
        warnings << "Unable to open the Windows ICMP interface.";
        return "Network test unavailable";
    }

    const IPAddr destination = inet_addr("1.1.1.1");
    const char payload[] = "MalachiteVR GameFix diagnostic";
    constexpr int attempts = 4;
    int received = 0;
    double totalLatency = 0.0;
    double minLatency = 10000.0;
    double maxLatency = 0.0;

    for (int i = 0; i < attempts; ++i) {
        char replyBuffer[sizeof(ICMP_ECHO_REPLY) + sizeof(payload) + 8]{};
        const DWORD replies = IcmpSendEcho(
            icmp,
            destination,
            const_cast<char *>(payload),
            static_cast<WORD>(sizeof(payload)),
            nullptr,
            replyBuffer,
            sizeof(replyBuffer),
            1000);

        if (replies == 0)
            continue;

        const auto *reply = reinterpret_cast<const ICMP_ECHO_REPLY *>(replyBuffer);
        if (reply->Status != IP_SUCCESS)
            continue;

        const double latency = static_cast<double>(reply->RoundTripTime);
        ++received;
        totalLatency += latency;
        minLatency = std::min(minLatency, latency);
        maxLatency = std::max(maxLatency, latency);
    }

    IcmpCloseHandle(icmp);

    const int lost = attempts - received;
    const double lossPercent = (static_cast<double>(lost) / attempts) * 100.0;
    if (lossPercent > 0.0)
        warnings << QString("ICMP packet loss detected: %1%.").arg(QString::number(lossPercent, 'f', 0));

    if (received == 0)
        return "No replies from 1.1.1.1 (ICMP may be blocked or the network may be unavailable)";

    const double average = totalLatency / received;
    const double jitter = maxLatency - minLatency;
    if (average >= 100.0)
        warnings << QString("High internet latency detected: %1 ms average.").arg(QString::number(average, 'f', 0));
    if (jitter >= 30.0)
        warnings << QString("High latency variation detected: %1 ms range.").arg(QString::number(jitter, 'f', 0));

    return QString("1.1.1.1: %1 ms average, %2 ms min, %3 ms max, %4% packet loss, %5 ms variation")
        .arg(QString::number(average, 'f', 1))
        .arg(QString::number(minLatency, 'f', 0))
        .arg(QString::number(maxLatency, 'f', 0))
        .arg(QString::number(lossPercent, 'f', 0))
        .arg(QString::number(jitter, 'f', 0));
}

QString storageCheck(QStringList &warnings)
{
    QStringList drives;
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((mask & (1u << i)) == 0)
            continue;

        wchar_t root[] = {static_cast<wchar_t>(L'A' + i), L':', L'\\', L'\0'};
        if (GetDriveTypeW(root) != DRIVE_FIXED)
            continue;

        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFreeBytes{};
        if (!GetDiskFreeSpaceExW(root, &freeBytes, &totalBytes, &totalFreeBytes) || totalBytes.QuadPart == 0)
            continue;

        const double freePercent = static_cast<double>(totalFreeBytes.QuadPart) /
                                   static_cast<double>(totalBytes.QuadPart) * 100.0;
        drives << QString("%1: %2 free of %3 (%4% free)")
                       .arg(QChar(static_cast<ushort>('A' + i)))
                       .arg(formatBytes(totalFreeBytes.QuadPart))
                       .arg(formatBytes(totalBytes.QuadPart))
                       .arg(QString::number(freePercent, 'f', 1));

        if (freePercent < 10.0)
            warnings << QString("Low free space on drive %1 (%2% remaining).")
                            .arg(QChar(static_cast<ushort>('A' + i)))
                            .arg(QString::number(freePercent, 'f', 1));
    }

    if (drives.isEmpty())
        return "No fixed storage volumes could be inspected";

    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    double diskActivity = -1.0;
    if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS &&
        PdhAddEnglishCounterW(query, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &counter) == ERROR_SUCCESS &&
        PdhCollectQueryData(query) == ERROR_SUCCESS) {
        Sleep(250);
        if (PdhCollectQueryData(query) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE value{};
            if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS)
                diskActivity = std::clamp(value.doubleValue, 0.0, 100.0);
        }
    }
    if (query)
        PdhCloseQuery(query);

    QString result = drives.join("\n");
    if (diskActivity >= 0.0) {
        result += QString("\nCurrent disk activity: %1%").arg(QString::number(diskActivity, 'f', 0));
        if (diskActivity >= 90.0)
            warnings << "Storage is currently under heavy activity.";
    }
    return result;
}

QString summarizeTelemetry(const TelemetrySnapshot &telemetry, QStringList &warnings)
{
    const quint64 usedRam = telemetry.totalMemoryBytes >= telemetry.availableMemoryBytes
                                ? telemetry.totalMemoryBytes - telemetry.availableMemoryBytes
                                : 0;
    const double ramPercent = telemetry.totalMemoryBytes > 0
                                  ? static_cast<double>(usedRam) / telemetry.totalMemoryBytes * 100.0
                                  : 0.0;
    const double vramPercent = telemetry.gpuMemoryTotalBytes > 0
                                   ? static_cast<double>(telemetry.gpuMemoryUsedBytes) /
                                         telemetry.gpuMemoryTotalBytes * 100.0
                                   : 0.0;

    if (ramPercent >= 90.0)
        warnings << QString("System RAM usage is very high (%1%).").arg(QString::number(ramPercent, 'f', 0));
    else if (ramPercent >= 80.0)
        warnings << QString("System RAM usage is elevated (%1%).").arg(QString::number(ramPercent, 'f', 0));

    if (vramPercent >= 90.0)
        warnings << QString("VRAM usage is very high (%1%).").arg(QString::number(vramPercent, 'f', 0));

    if (telemetry.gpuTemperatureAvailable && telemetry.gpuTemperatureCelsius >= 85.0)
        warnings << QString("GPU temperature is high (%1 °C).").arg(QString::number(telemetry.gpuTemperatureCelsius, 'f', 0));
    else if (telemetry.gpuTemperatureAvailable && telemetry.gpuTemperatureCelsius >= 80.0)
        warnings << QString("GPU temperature is elevated (%1 °C).").arg(QString::number(telemetry.gpuTemperatureCelsius, 'f', 0));

    return QString("CPU %1% | GPU %2% | RAM %3% | VRAM %4% | GPU temperature %5")
        .arg(QString::number(telemetry.cpuUsagePercent, 'f', 0))
        .arg(telemetry.gpuUsageAvailable ? QString::number(telemetry.gpuUsagePercent, 'f', 0) + "%" : "unavailable")
        .arg(QString::number(ramPercent, 'f', 0))
        .arg(telemetry.gpuMemoryTotalBytes > 0 ? QString::number(vramPercent, 'f', 0) + "%" : "unavailable")
        .arg(telemetry.gpuTemperatureAvailable
                 ? QString::number(telemetry.gpuTemperatureCelsius, 'f', 0) + " °C"
                 : "unavailable");
}
}
#endif

DiagnosticReport runFullDiagnostic()
{
    DiagnosticReport report;

#ifdef Q_OS_WIN
    const SystemInfo system = collectSystemInfo();
    const TelemetrySnapshot telemetry = collectTelemetry();
    QStringList warnings;

    const QString game = detectRunningGame();
    const QString telemetrySummary = summarizeTelemetry(telemetry, warnings);
    const QString bottleneck = diagnoseBottleneck(telemetry);
    const QString network = networkCheck(warnings);
    const QString storage = storageCheck(warnings);

    QStringList details;
    details << "MALACHITEVR GAMEFIX - FULL DIAGNOSTIC"
            << "====================================="
            << ""
            << "SYSTEM"
            << QString("OS: %1").arg(system.operatingSystem)
            << QString("Architecture: %1").arg(system.architecture)
            << QString("Logical processors: %1").arg(system.logicalProcessors)
            << QString("GPU: %1").arg(system.graphics)
            << QString("RAM: %1 total, %2 available")
                   .arg(formatBytes(system.totalMemoryBytes))
                   .arg(formatBytes(system.availableMemoryBytes))
            << ""
            << "RUNNING GAME"
            << game
            << ""
            << "LIVE PERFORMANCE SNAPSHOT"
            << telemetrySummary
            << QString("Diagnosis: %1").arg(bottleneck)
            << ""
            << "NETWORK"
            << network
            << ""
            << "STORAGE"
            << storage
            << ""
            << "WARNINGS"
            << (warnings.isEmpty() ? "No immediate warnings detected." : warnings.join("\n"));

    report.details = details.join("\n");
    report.summary = warnings.isEmpty()
                         ? "Diagnostic complete: no immediate problems detected."
                         : QString("Diagnostic complete: %1 warning(s) detected.").arg(warnings.size());
#else
    report.summary = "Full diagnostics are currently implemented for Windows.";
    report.details = report.summary;
#endif

    return report;
}
