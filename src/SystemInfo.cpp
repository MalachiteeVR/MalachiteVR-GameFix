#include "SystemInfo.h"

#include <QSysInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi.h>
#endif

#include <algorithm>

QString formatBytes(quint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;

    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }

    if (unit == 0)
        return QString::number(static_cast<qulonglong>(bytes)) + " " + units[unit];

    return QString::number(value, 'f', value >= 10.0 ? 1 : 2) + " " + units[unit];
}

#ifdef Q_OS_WIN
static QString detectGraphicsAdapter()
{
    IDXGIFactory *factory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory))))
        return "Unavailable";

    QString result = "Unavailable";
    IDXGIAdapter *adapter = nullptr;

    if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(adapter->GetDesc(&desc)))
            result = QString::fromWCharArray(desc.Description);
        adapter->Release();
    }

    factory->Release();
    return result;
}
#endif

SystemInfo collectSystemInfo()
{
    SystemInfo info;
    info.operatingSystem = QSysInfo::prettyProductName();
    info.architecture = QSysInfo::currentCpuArchitecture();
    info.cpu = QSysInfo::buildCpuArchitecture();

#ifdef Q_OS_WIN
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    info.logicalProcessors = static_cast<int>(systemInfo.dwNumberOfProcessors);

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        info.totalMemoryBytes = memory.ullTotalPhys;
        info.availableMemoryBytes = memory.ullAvailPhys;
    }

    info.graphics = detectGraphicsAdapter();
#else
    info.logicalProcessors = QSysInfo::machineHostName().isEmpty() ? 0 : 1;
#endif

    return info;
}
