#include "Telemetry.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QVector>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#endif

namespace
{
#ifdef Q_OS_WIN

double readCounter(PDH_HCOUNTER counter, bool &available)
{
    available = false;
    if (!counter)
        return 0.0;

    if (PdhCollectQueryData(nullptr) != ERROR_SUCCESS)
        return 0.0;

    PDH_FMT_COUNTERVALUE value{};
    if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) != ERROR_SUCCESS)
        return 0.0;

    available = value.CStatus == ERROR_SUCCESS;
    return available ? std::clamp(value.doubleValue, 0.0, 100.0) : 0.0;
}

double readTotalCpuUsage(bool &available)
{
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
            return 0.0;

        const PDH_STATUS status = PdhAddEnglishCounterW(
            query,
            L"\\Processor(_Total)\\% Processor Time",
            0,
            &counter);
        if (status != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            return 0.0;
        }

        PdhCollectQueryData(query);
        available = false;
        return 0.0;
    }

    if (!query || !counter || PdhCollectQueryData(query) != ERROR_SUCCESS) {
        available = false;
        return 0.0;
    }

    PDH_FMT_COUNTERVALUE value{};
    const PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value);
    available = status == ERROR_SUCCESS && value.CStatus == ERROR_SUCCESS;
    return available ? std::clamp(value.doubleValue, 0.0, 100.0) : 0.0;
}

QVector<double> readPerCoreUsage(bool &available)
{
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) {
            available = false;
            return {};
        }

        const PDH_STATUS status = PdhAddEnglishCounterW(
            query,
            L"\\Processor(*)\\% Processor Time",
            0,
            &counter);
        if (status != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = nullptr;
            available = false;
            return {};
        }

        PdhCollectQueryData(query);
        available = false;
        return {};
    }

    if (!query || !counter || PdhCollectQueryData(query) != ERROR_SUCCESS) {
        available = false;
        return {};
    }

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(
        counter,
        PDH_FMT_DOUBLE,
        &bufferSize,
        &itemCount,
        nullptr);
    if (status != PDH_MORE_DATA || bufferSize == 0 || itemCount == 0) {
        available = false;
        return {};
    }

    QByteArray buffer(static_cast<qsizetype>(bufferSize), Qt::Uninitialized);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
    status = PdhGetFormattedCounterArrayW(
        counter,
        PDH_FMT_DOUBLE,
        &bufferSize,
        &itemCount,
        items);
    if (status != ERROR_SUCCESS) {
        available = false;
        return {};
    }

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
    used = 0;
    total = 0;

    IDXGIFactory1 *factory = nullptr;
    if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) != S_OK)
        return;

    IDXGIAdapter1 *adapter = nullptr;
    for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_ADAPTER_DESC1 desc{};
        if (adapter->GetDesc1(&desc) == S_OK && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            IDXGIAdapter3 *adapter3 = nullptr;
            if (adapter->QueryInterface(IID_PPV_ARGS(&adapter3)) == S_OK) {
                DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                if (adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info) == S_OK) {
                    used = info.CurrentUsage;
                    total = info.Budget;
                }
                adapter3->Release();
            }
            adapter->Release();
            break;
        }
        adapter->Release();
        adapter = nullptr;
    }

    factory->Release();
}
#endif
}

TelemetrySnapshot collectTelemetry()
{
    TelemetrySnapshot snapshot;

#ifdef Q_OS_WIN
    bool cpuAvailable = false;
    snapshot.cpuUsagePercent = readTotalCpuUsage(cpuAvailable);
    snapshot.cpuUsageAvailable = cpuAvailable;

    bool coresAvailable = false;
    snapshot.perCoreUsage = readPerCoreUsage(coresAvailable);
    snapshot.perCoreUsageAvailable = coresAvailable;

    readGpuMemory(snapshot.vramUsedBytes, snapshot.vramTotalBytes);
    snapshot.vramAvailable = snapshot.vramTotalBytes > 0;

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        snapshot.ramTotalBytes = memory.ullTotalPhys;
        snapshot.ramAvailableBytes = memory.ullAvailPhys;
        snapshot.ramUsagePercent = static_cast<double>(memory.dwMemoryLoad);
        snapshot.ramAvailable = true;
    }

    snapshot.timestamp = QDateTime::currentDateTime();
#endif

    return snapshot;
}
