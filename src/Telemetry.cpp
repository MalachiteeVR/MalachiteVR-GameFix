#include "Telemetry.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <pdh.h>
#include <pdhmsg.h>
#endif

#include <QByteArray>

#include <algorithm>

#ifdef Q_OS_WIN
namespace
{
struct CpuTimes
{
    quint64 idle = 0;
    quint64 kernel = 0;
    quint64 user = 0;
};

CpuTimes readCpuTimes()
{
    FILETIME idleTime{}, kernelTime{}, userTime{};
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return {};

    ULARGE_INTEGER idle{}, kernel{}, user{};
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    return {idle.QuadPart, kernel.QuadPart, user.QuadPart};
}

double readCpuUsage()
{
    static CpuTimes previous{};
    static bool initialized = false;

    const CpuTimes current = readCpuTimes();
    if (!initialized) {
        previous = current;
        initialized = true;
        return 0.0;
    }

    const quint64 idleDelta = current.idle - previous.idle;
    const quint64 kernelDelta = current.kernel - previous.kernel;
    const quint64 userDelta = current.user - previous.user;
    const quint64 totalDelta = kernelDelta + userDelta;
    previous = current;

    if (totalDelta == 0)
        return 0.0;

    const double busy = static_cast<double>(totalDelta - std::min(idleDelta, totalDelta));
    return std::clamp((busy * 100.0) / static_cast<double>(totalDelta), 0.0, 100.0);
}

double readGpuUsage(bool &available)
{
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
            return available = false, 0.0;

        const PDH_STATUS status = PdhAddEnglishCounterW(
            query,
            L"\\GPU Engine(*)\\Utilization Percentage",
            0,
            &counter);
        if (status != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            return available = false, 0.0;
        }

        PdhCollectQueryData(query);
        return available = false, 0.0;
    }

    if (!query || !counter || PdhCollectQueryData(query) != ERROR_SUCCESS)
        return available = false, 0.0;

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(
        counter,
        PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
        &bufferSize,
        &itemCount,
        nullptr);

    if (status != PDH_MORE_DATA || bufferSize == 0 || itemCount == 0)
        return available = false, 0.0;

    QByteArray buffer(static_cast<qsizetype>(bufferSize), Qt::Uninitialized);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());

    status = PdhGetFormattedCounterArrayW(
        counter,
        PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
        &bufferSize,
        &itemCount,
        items);
    if (status != ERROR_SUCCESS)
        return available = false, 0.0;

    double total = 0.0;
    for (DWORD i = 0; i < itemCount; ++i) {
        if (items[i].FmtValue.CStatus == ERROR_SUCCESS)
            total += items[i].FmtValue.doubleValue;
    }

    available = true;
    return std::clamp(total, 0.0, 100.0);
}

QVector<double> readPerCoreUsage(bool &available)
{
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
            return available = false, {};

        const PDH_STATUS status = PdhAddEnglishCounterW(
            query,
            L"\\Processor(*)\\% Processor Time",
            0,
            &counter);
        if (status != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            return available = false, {};
        }

        PdhCollectQueryData(query);
        return available = false, {};
    }

    if (!query || !counter || PdhCollectQueryData(query) != ERROR_SUCCESS)
        return available = false, {};

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(
        counter,
        PDH_FMT_DOUBLE,
        &bufferSize,
        &itemCount,
        nullptr);
    if (status != PDH_MORE_DATA || bufferSize == 0 || itemCount == 0)
        return available = false, {};

    QByteArray buffer(static_cast<qsizetype>(bufferSize), Qt::Uninitialized);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
    status = PdhGetFormattedCounterArrayW(
        counter,
        PDH_FMT_DOUBLE,
        &bufferSize,
        &itemCount,
        items);
    if (status != ERROR_SUCCESS)
        return available = false, {};

    QVector<double> cores;
    cores.reserve(static_cast<int>(itemCount));
    for (DWORD i = 0; i < itemCount; ++i) {
        const QString name = QString::fromWCharArray(items[i].szName);
        if (name.compare("_Total", Qt::CaseInsensitive) == 0)
            continue;
        if (items[i].FmtValue.CStatus == ERROR_SUCCESS)
            cores.append(std::clamp(items[i].FmtValue.doubleValue, 0.0, 100.0));
    }

    available = !cores.isEmpty();
    return cores;
}

void readGpuMemory(quint64 &used, quint64 &total)
{
    IDXGIFactory *factory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory))))
        return;

    IDXGIAdapter *adapter = nullptr;
    if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(adapter->GetDesc(&desc)))
            total = desc.DedicatedVideoMemory;

        IDXGIAdapter3 *adapter3 = nullptr;
        if (SUCCEEDED(adapter->QueryInterface(
                __uuidof(IDXGIAdapter3),
                reinterpret_cast<void **>(&adapter3)))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo{};
            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
                    0,
                    DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
                    &memoryInfo))) {
                used = memoryInfo.CurrentUsage;
            }
            adapter3->Release();
        }

        adapter->Release();
    }

    factory->Release();
}

double readNvidiaGpuTemperature(bool &available)
{
    using NvmlDevice = void *;
    using NvmlReturn = int;
    using NvmlInit = NvmlReturn (*)();
    using NvmlGetHandle = NvmlReturn (*)(unsigned int, NvmlDevice *);
    using NvmlGetTemperature = NvmlReturn (*)(NvmlDevice, int, unsigned int *);
    using NvmlShutdown = NvmlReturn (*)();

    HMODULE nvml = LoadLibraryW(L"nvml.dll");
    if (!nvml)
        return available = false, 0.0;

    const auto init = reinterpret_cast<NvmlInit>(GetProcAddress(nvml, "nvmlInit_v2"));
    const auto getHandle = reinterpret_cast<NvmlGetHandle>(
        GetProcAddress(nvml, "nvmlDeviceGetHandleByIndex_v2"));
    const auto getTemperature = reinterpret_cast<NvmlGetTemperature>(
        GetProcAddress(nvml, "nvmlDeviceGetTemperature"));
    const auto shutdown = reinterpret_cast<NvmlShutdown>(GetProcAddress(nvml, "nvmlShutdown"));

    if (!init || !getHandle || !getTemperature || !shutdown || init() != 0) {
        FreeLibrary(nvml);
        return available = false, 0.0;
    }

    NvmlDevice device = nullptr;
    unsigned int temperature = 0;
    const bool success = getHandle(0, &device) == 0 &&
                         getTemperature(device, 0, &temperature) == 0;
    shutdown();
    FreeLibrary(nvml);

    if (!success)
        return available = false, 0.0;

    available = true;
    return static_cast<double>(temperature);
}
}
#endif

TelemetrySnapshot collectTelemetry()
{
    TelemetrySnapshot telemetry;

#ifdef Q_OS_WIN
    telemetry.cpuUsagePercent = readCpuUsage();

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        telemetry.totalMemoryBytes = memory.ullTotalPhys;
        telemetry.availableMemoryBytes = memory.ullAvailPhys;
    }

    readGpuMemory(telemetry.gpuMemoryUsedBytes, telemetry.gpuMemoryTotalBytes);
    telemetry.gpuUsagePercent = readGpuUsage(telemetry.gpuUsageAvailable);
    telemetry.perCoreUsagePercent = readPerCoreUsage(telemetry.perCoreUsageAvailable);
    telemetry.gpuTemperatureCelsius = readNvidiaGpuTemperature(telemetry.gpuTemperatureAvailable);
#endif

    return telemetry;
}

QString diagnoseBottleneck(const TelemetrySnapshot &telemetry)
{
    const double memoryUsed = telemetry.totalMemoryBytes > 0
                                  ? (1.0 - static_cast<double>(telemetry.availableMemoryBytes) /
                                           static_cast<double>(telemetry.totalMemoryBytes)) * 100.0
                                  : 0.0;
    const double vramUsed = telemetry.gpuMemoryTotalBytes > 0
                                ? (static_cast<double>(telemetry.gpuMemoryUsedBytes) /
                                   static_cast<double>(telemetry.gpuMemoryTotalBytes)) * 100.0
                                : 0.0;

    double hottestCore = 0.0;
    for (const double core : telemetry.perCoreUsagePercent)
        hottestCore = std::max(hottestCore, core);

    if (memoryUsed >= 90.0)
        return QString("System RAM pressure detected (%1% used). Close background applications or reduce in-game memory usage.")
            .arg(QString::number(memoryUsed, 'f', 0));

    if (telemetry.gpuUsageAvailable && telemetry.gpuUsagePercent >= 90.0 && vramUsed >= 90.0)
        return QString("GPU and VRAM are heavily loaded (%1% GPU, %2% VRAM). The game is likely GPU/VRAM-bound.")
            .arg(QString::number(telemetry.gpuUsagePercent, 'f', 0))
            .arg(QString::number(vramUsed, 'f', 0));

    if (telemetry.gpuUsageAvailable && telemetry.gpuUsagePercent >= 90.0 && telemetry.cpuUsagePercent < 80.0)
        return QString("GPU utilization is high while CPU utilization is lower. The game is likely GPU-bound.");

    if (telemetry.cpuUsagePercent >= 90.0 && telemetry.gpuUsageAvailable && telemetry.gpuUsagePercent < 80.0)
        return QString("CPU utilization is high while GPU utilization is lower. The game is likely CPU-bound.");

    if (hottestCore >= 95.0 && telemetry.cpuUsagePercent < 85.0)
        return QString("One or more CPU cores are saturated (hottest core %1%). A lightly-threaded game may be CPU-bound even when total CPU usage looks moderate.")
            .arg(QString::number(hottestCore, 'f', 0));

    if (memoryUsed >= 80.0)
        return QString("System RAM usage is elevated (%1%). Background applications may be affecting performance.")
            .arg(QString::number(memoryUsed, 'f', 0));

    return QString("No obvious system bottleneck detected right now. This is a heuristic based on current utilization.");
}
