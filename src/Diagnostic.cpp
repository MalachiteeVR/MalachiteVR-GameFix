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
#include <ws2tcpip.h>
#endif

#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
namespace
{
struct ProcessInfo
{
    QString name;
    QString path;
};

ProcessInfo processInfoFromPid(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};

    wchar_t path[32768]{};
    DWORD length = static_cast<DWORD>(std::size(path));
    ProcessInfo result;
    if (QueryFullProcessImageNameW(process, 0, path, &length)) {
        result.path = QString::fromWCharArray(path, static_cast<int>(length));
        const int slash = result.path.lastIndexOf(QChar('\\'));
        result.name = slash >= 0 ? result.path.mid(slash + 1) : result.path;
    }
    CloseHandle(process);
    return result;
}

QString friendlyGameName(const QString &processName, const QString &windowTitle)
{
    const QString process = processName.toLower();
    const QString title = windowTitle.toLower();

    if (process == "java.exe" || process == "javaw.exe") {
        if (title.contains("minecraft"))
            return "Minecraft Java Edition";
        return {};
    }
    if (process == "minecraftlauncher.exe") return "Minecraft Launcher";
    if (process == "vrchat.exe" || title.contains("vrchat")) return "VRChat";
    if (process == "beatsaber.exe" || title.contains("beat saber")) return "Beat Saber";
    if (process == "eldenring.exe" || title.contains("elden ring")) return "Elden Ring";
    if (process == "fortniteclient-win64-shipping.exe" || title.contains("fortnite")) return "Fortnite";
    if (process == "cs2.exe" || process == "csgo.exe" || title.contains("counter-strike")) return "Counter-Strike";
    if (process == "valorant-win64-shipping.exe" || title.contains("valorant")) return "VALORANT";
    if (process == "helldivers2.exe" || title.contains("helldivers")) return "HELLDIVERS 2";
    if (process == "palworld-win64-shipping.exe" || title.contains("palworld")) return "Palworld";
    if (process == "r5apex.exe" || title.contains("apex legends")) return "Apex Legends";
    if (process == "starfield.exe" || title.contains("starfield")) return "Starfield";
    if (process == "cyberpunk2077.exe" || title.contains("cyberpunk")) return "Cyberpunk 2077";
    if (process == "gta5.exe" || title.contains("grand theft auto")) return "Grand Theft Auto V";
    if (process == "rocketleague.exe" || title.contains("rocket league")) return "Rocket League";
    if (process == "overwatch.exe" || title.contains("overwatch")) return "Overwatch";
    return {};
}

bool isKnownProcess(const QString &processName)
{
    static const QStringList known = {
        "vrchat.exe", "beatsaber.exe", "minecraft.exe", "minecraftlauncher.exe",
        "javaw.exe", "java.exe", "eldenring.exe", "fortniteclient-win64-shipping.exe",
        "cs2.exe", "csgo.exe", "valorant-win64-shipping.exe", "helldivers2.exe",
        "palworld-win64-shipping.exe", "r5apex.exe", "starfield.exe", "cyberpunk2077.exe",
        "gta5.exe", "rocketleague.exe", "overwatch.exe"
    };
    return known.contains(processName.toLower());
}

QString windowTitleForProcess(DWORD pid)
{
    struct Context {
        DWORD pid = 0;
        QString title;
    } context{pid, {}};

    EnumWindows([](HWND hwnd, LPARAM parameter) -> BOOL {
        auto *context = reinterpret_cast<Context *>(parameter);
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid != context->pid || !IsWindowVisible(hwnd))
            return TRUE;

        wchar_t title[512]{};
        const int length = GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
        if (length > 0) {
            context->title = QString::fromWCharArray(title, length);
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    return context.title;
}

struct WindowSearchContext
{
    DetectedGame game;
};

BOOL CALLBACK findGameWindow(HWND hwnd, LPARAM parameter)
{
    auto *context = reinterpret_cast<WindowSearchContext *>(parameter);
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
        return TRUE;

    wchar_t titleBuffer[512]{};
    const int titleLength = GetWindowTextW(hwnd, titleBuffer, static_cast<int>(std::size(titleBuffer)));
    if (titleLength <= 0)
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    const ProcessInfo process = processInfoFromPid(pid);
    const QString title = QString::fromWCharArray(titleBuffer, titleLength);
    const QString friendly = friendlyGameName(process.name, title);
    if (friendly.isEmpty())
        return TRUE;

    context->game.name = friendly;
    context->game.processName = process.name;
    context->game.executablePath = process.path;
    context->game.processId = pid;
    context->game.running = true;
    return FALSE;
}

DetectedGame detectRunningGameImpl()
{
    const HWND foreground = GetForegroundWindow();
    if (foreground) {
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        const ProcessInfo process = processInfoFromPid(pid);
        const QString title = windowTitleForProcess(pid);
        const QString friendly = friendlyGameName(process.name, title);
        if (!friendly.isEmpty() || isKnownProcess(process.name)) {
            DetectedGame game;
            game.name = friendly.isEmpty() ? process.name : friendly;
            game.processName = process.name;
            game.executablePath = process.path;
            game.processId = pid;
            game.running = true;
            return game;
        }
    }

    WindowSearchContext windowContext;
    EnumWindows(findGameWindow, reinterpret_cast<LPARAM>(&windowContext));
    if (windowContext.game.running)
        return windowContext.game;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                const QString processName = QString::fromWCharArray(entry.szExeFile);
                if (!isKnownProcess(processName))
                    continue;

                const ProcessInfo process = processInfoFromPid(entry.th32ProcessID);
                const QString friendly = friendlyGameName(process.name, windowTitleForProcess(entry.th32ProcessID));
                if (friendly.isEmpty())
                    continue;

                DetectedGame game;
                game.name = friendly;
                game.processName = process.name;
                game.executablePath = process.path;
                game.processId = entry.th32ProcessID;
                game.running = true;
                CloseHandle(snapshot);
                return game;
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    return {};
}

QString addressToString(const SOCKADDR *address)
{
    if (!address)
        return {};

    wchar_t buffer[INET6_ADDRSTRLEN]{};
    if (address->sa_family == AF_INET) {
        const auto *ipv4 = reinterpret_cast<const SOCKADDR_IN *>(address);
        return InetNtopW(AF_INET, const_cast<IN_ADDR *>(&ipv4->sin_addr), buffer,
                         static_cast<DWORD>(std::size(buffer)))
                   ? QString::fromWCharArray(buffer)
                   : QString{};
    }
    if (address->sa_family == AF_INET6) {
        const auto *ipv6 = reinterpret_cast<const SOCKADDR_IN6 *>(address);
        return InetNtopW(AF_INET6, const_cast<IN6_ADDR *>(&ipv6->sin6_addr), buffer,
                         static_cast<DWORD>(std::size(buffer)))
                   ? QString::fromWCharArray(buffer)
                   : QString{};
    }
    return {};
}

QString formatLinkSpeed(ULONG64 bitsPerSecond)
{
    if (bitsPerSecond == 0)
        return "unknown";
    if (bitsPerSecond >= 1000000000ULL)
        return QString("%1 Gbps").arg(QString::number(bitsPerSecond / 1000000000.0, 'f', 1));
    return QString("%1 Mbps").arg(QString::number(bitsPerSecond / 1000000.0, 'f', 0));
}

QString networkAdapterCheck(QStringList &warnings, QString &gateway)
{
    ULONG size = 15000;
    std::vector<BYTE> buffer(size);
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
    ULONG status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    if (status == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
    }

    if (status != NO_ERROR)
        return "Unable to query Windows network adapters.";

    QStringList adapters;
    for (PIP_ADAPTER_ADDRESSES adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
         adapter != nullptr; adapter = adapter->Next) {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;

        const QString name = adapter->FriendlyName
                                 ? QString::fromWCharArray(adapter->FriendlyName)
                                 : QStringLiteral("Unnamed adapter");
        const QString state = adapter->OperStatus == IfOperStatusUp ? "Up" : "Down";
        QStringList addresses;
        for (PIP_ADAPTER_UNICAST_ADDRESS address = adapter->FirstUnicastAddress;
             address != nullptr; address = address->Next) {
            const QString text = addressToString(address->Address.lpSockaddr);
            if (!text.isEmpty())
                addresses << text;
        }

        QString adapterGateway;
        for (PIP_ADAPTER_GATEWAY_ADDRESS_LH gatewayAddress = adapter->FirstGatewayAddress;
             gatewayAddress != nullptr; gatewayAddress = gatewayAddress->Next) {
            adapterGateway = addressToString(gatewayAddress->Address.lpSockaddr);
            if (adapterGateway.contains('.'))
                break;
        }
        if (gateway.isEmpty() && !adapterGateway.isEmpty())
            gateway = adapterGateway;

        adapters << QString("%1: %2 | RX %3 | TX %4 | IP %5%6")
                        .arg(name)
                        .arg(state)
                        .arg(formatLinkSpeed(adapter->ReceiveLinkSpeed))
                        .arg(formatLinkSpeed(adapter->TransmitLinkSpeed))
                        .arg(addresses.isEmpty() ? "none" : addresses.join(", "))
                        .arg(adapterGateway.isEmpty() ? QString{} : QString(" | Gateway %1").arg(adapterGateway));

        if (adapter->OperStatus == IfOperStatusUp &&
            (adapter->ReceiveLinkSpeed > 0 && adapter->ReceiveLinkSpeed < 100000000ULL))
            warnings << QString("Active adapter '%1' has a link speed below 100 Mbps.").arg(name);
    }

    if (adapters.isEmpty())
        warnings << "No active physical network adapter was found.";
    return adapters.isEmpty() ? "No network adapters could be inspected." : adapters.join("\n");
}

bool pingHost(const QString &host, int attempts, QString &result, double &average, double &jitter, int &loss)
{
    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE)
        return false;

    const QByteArray hostBytes = host.toLatin1();
    const IPAddr destination = inet_addr(hostBytes.constData());
    const char payload[] = "MalachiteVR GameFix diagnostic";
    int received = 0;
    double total = 0.0;
    double minimum = 100000.0;
    double maximum = 0.0;

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
        total += latency;
        minimum = std::min(minimum, latency);
        maximum = std::max(maximum, latency);
    }

    IcmpCloseHandle(icmp);
    loss = attempts - received;
    if (received == 0)
        return false;

    average = total / received;
    jitter = maximum - minimum;
    result = QString("%1 ms average, %2 ms min, %3 ms max, %4% packet loss, %5 ms variation")
                 .arg(QString::number(average, 'f', 1))
                 .arg(QString::number(minimum, 'f', 0))
                 .arg(QString::number(maximum, 'f', 0))
                 .arg(QString::number(static_cast<double>(loss) / attempts * 100.0, 'f', 0))
                 .arg(QString::number(jitter, 'f', 0));
    return true;
}

QString networkCheck(QStringList &warnings, const QString &gateway)
{
    QStringList results;
    if (!gateway.isEmpty() && gateway.contains('.')) {
        QString gatewayResult;
        double average = 0.0;
        double jitter = 0.0;
        int loss = 0;
        if (pingHost(gateway, 2, gatewayResult, average, jitter, loss)) {
            results << QString("Default gateway %1: %2").arg(gateway, gatewayResult);
            if (loss > 0 || average >= 20.0 || jitter >= 10.0)
                warnings << QString("Local gateway latency/loss is elevated: %1.").arg(gatewayResult);
        } else {
            warnings << QString("No ICMP reply from the default gateway (%1).").arg(gateway);
        }
    }

    QString internetResult;
    double average = 0.0;
    double jitter = 0.0;
    int loss = 0;
    if (!pingHost("1.1.1.1", 4, internetResult, average, jitter, loss)) {
        warnings << "No replies from 1.1.1.1. ICMP may be blocked or the network may be unavailable.";
        results << "1.1.1.1: no ICMP replies";
    } else {
        results << QString("Internet 1.1.1.1: %1").arg(internetResult);
        if (loss > 0)
            warnings << QString("Internet ICMP packet loss detected: %1%.").arg(QString::number(static_cast<double>(loss) / 4.0 * 100.0, 'f', 0));
        if (average >= 100.0)
            warnings << QString("High internet latency detected: %1 ms average.").arg(QString::number(average, 'f', 0));
        if (jitter >= 30.0)
            warnings << QString("High internet latency variation detected: %1 ms range.").arg(QString::number(jitter, 'f', 0));
    }

    return results.isEmpty() ? "Network test unavailable" : results.join("\n");
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

QString readCpuTemperature(QString &source)
{
    QProcess process;
    const QString command =
        "$values = Get-CimInstance -Namespace root/wmi -ClassName MSAcpi_ThermalZoneTemperature "
        "-ErrorAction SilentlyContinue | Where-Object { $_.CurrentTemperature -gt 0 } | "
        "ForEach-Object { [math]::Round(($_.CurrentTemperature / 10) - 273.15, 1) }; "
        "if ($values) { $values -join ',' }";
    process.start("powershell.exe", {"-NoProfile", "-NonInteractive", "-Command", command});
    if (!process.waitForFinished(1500)) {
        process.kill();
        return {};
    }

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    const QStringList values = output.split(',', Qt::SkipEmptyParts);
    for (const QString &value : values) {
        bool ok = false;
        const double temperature = value.trimmed().toDouble(&ok);
        if (ok && temperature > -20.0 && temperature < 130.0) {
            source = "Windows ACPI thermal zone";
            return QString::number(temperature, 'f', 1);
        }
    }
    return {};
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

    double hottestCore = 0.0;
    int hottestCoreIndex = -1;
    for (int i = 0; i < telemetry.perCoreUsagePercent.size(); ++i) {
        if (telemetry.perCoreUsagePercent.at(i) > hottestCore) {
            hottestCore = telemetry.perCoreUsagePercent.at(i);
            hottestCoreIndex = i;
        }
    }
    if (hottestCore >= 95.0)
        warnings << QString("CPU core %1 is heavily loaded (%2%).")
                        .arg(hottestCoreIndex + 1)
                        .arg(QString::number(hottestCore, 'f', 0));

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

DetectedGame detectRunningGame()
{
#ifdef Q_OS_WIN
    return detectRunningGameImpl();
#else
    return {};
#endif
}

DiagnosticReport runFullDiagnostic()
{
    DiagnosticReport report;

#ifdef Q_OS_WIN
    const SystemInfo system = collectSystemInfo();
    const TelemetrySnapshot telemetry = collectTelemetry();
    QStringList warnings;

    report.game = detectRunningGameImpl();
    const QString game = report.game.running
                             ? QString("%1 (%2)").arg(report.game.name, report.game.processName)
                             : "No recognized game is currently running";
    const QString telemetrySummary = summarizeTelemetry(telemetry, warnings);
    const QString bottleneck = diagnoseBottleneck(telemetry);

    QString gateway;
    const QString adapters = networkAdapterCheck(warnings, gateway);
    const QString network = networkCheck(warnings, gateway);
    const QString storage = storageCheck(warnings);

    QString cpuTemperatureSource;
    const QString cpuTemperature = readCpuTemperature(cpuTemperatureSource);
    if (!cpuTemperature.isEmpty()) {
        const double temperature = cpuTemperature.toDouble();
        if (temperature >= 90.0)
            warnings << QString("Reported %1 CPU/thermal-zone temperature is high (%2 °C).")
                            .arg(cpuTemperatureSource)
                            .arg(cpuTemperature);
        else if (temperature >= 80.0)
            warnings << QString("Reported %1 CPU/thermal-zone temperature is elevated (%2 °C).")
                            .arg(cpuTemperatureSource)
                            .arg(cpuTemperature);
    }

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
                   .arg(formatBytes(system.availableMemoryBytes));

    if (!cpuTemperature.isEmpty())
        details << QString("CPU/thermal-zone temperature: %1 °C (%2)").arg(cpuTemperature, cpuTemperatureSource);
    else
        details << "CPU temperature: unavailable through Windows ACPI thermal-zone telemetry";

    details << ""
            << "RUNNING GAME"
            << game;
    if (report.game.running)
        details << QString("Executable: %1").arg(report.game.executablePath.isEmpty() ? "unavailable" : report.game.executablePath);

    details << ""
            << "LIVE PERFORMANCE SNAPSHOT"
            << telemetrySummary
            << QString("Diagnosis: %1").arg(bottleneck);

    if (telemetry.perCoreUsageAvailable) {
        QStringList cores;
        for (int i = 0; i < telemetry.perCoreUsagePercent.size(); ++i)
            cores << QString("Core %1: %2%").arg(i + 1).arg(QString::number(telemetry.perCoreUsagePercent.at(i), 'f', 0));
        details << "Per-core CPU utilization:" << cores.join(" | ");
    } else {
        details << "Per-core CPU utilization: unavailable";
    }

    details << ""
            << "NETWORK ADAPTERS / LINK"
            << adapters
            << ""
            << "NETWORK LATENCY / LOSS"
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
