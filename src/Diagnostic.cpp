#include "Diagnostic.h"
#include "SystemInfo.h"
#include "Telemetry.h"

#include <QDateTime>
#include <QProcess>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <netioapi.h>
#include <pdh.h>
#endif

namespace
{
#ifdef Q_OS_WIN
QString processNameFromPid(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};

    wchar_t path[32768]{};
    DWORD length = static_cast<DWORD>(std::size(path));
    QString result;
    if (QueryFullProcessImageNameW(process, 0, path, &length))
        result = QString::fromWCharArray(path, static_cast<int>(length));
    CloseHandle(process);
    return result;
}

QString detectRunningGame(QString &executablePath)
{
    static const QStringList knownGames = {
        "vrchat.exe", "beatsaber.exe", "minecraft.exe", "minecraftlauncher.exe",
        "eldenring.exe", "fortniteclient-win64-shipping.exe", "cs2.exe", "csgo.exe",
        "valorant-win64-shipping.exe", "helldivers2.exe", "palworld-win64-shipping.exe",
        "r5apex.exe", "starfield.exe", "cyberpunk2077.exe", "gta5.exe",
        "rocketleague.exe", "overwatch.exe"
    };

    const DWORD foregroundPid = [] {
        HWND window = GetForegroundWindow();
        DWORD pid = 0;
        if (window)
            GetWindowThreadProcessId(window, &pid);
        return pid;
    }();

    if (foregroundPid != 0) {
        const QString foregroundPath = processNameFromPid(foregroundPid);
        const QString foregroundName = foregroundPath.section('\\', -1).toLower();
        if (knownGames.contains(foregroundName)) {
            executablePath = foregroundPath;
            return foregroundName;
        }
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return {};

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    QString detected;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
            if (knownGames.contains(name)) {
                executablePath = processNameFromPid(entry.th32ProcessID);
                detected = name;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return detected;
}

QString networkAdapterReport(QStringList &warnings)
{
    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &size);
    if (size == 0)
        return "No network adapters could be inspected.";

    QByteArray buffer(static_cast<qsizetype>(size), Qt::Uninitialized);
    auto *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &size) != NO_ERROR)
        return "Windows could not enumerate network adapters.";

    QStringList lines;
    for (const IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        QString name = QString::fromWCharArray(adapter->FriendlyName ? adapter->FriendlyName : L"");
        QStringList addresses;
        for (const IP_ADAPTER_UNICAST_ADDRESS *address = adapter->FirstUnicastAddress; address; address = address->Next) {
            char host[NI_MAXHOST]{};
            if (getnameinfo(address->Address.lpSockaddr,
                            static_cast<socklen_t>(address->Address.iSockaddrLength),
                            host, sizeof(host), nullptr, 0, NI_NUMERICHOST) == 0)
                addresses << QString::fromLocal8Bit(host);
        }

        QString gateway;
        if (adapter->FirstGatewayAddress) {
            char host[NI_MAXHOST]{};
            if (getnameinfo(adapter->FirstGatewayAddress->Address.lpSockaddr,
                            static_cast<socklen_t>(adapter->FirstGatewayAddress->Address.iSockaddrLength),
                            host, sizeof(host), nullptr, 0, NI_NUMERICHOST) == 0)
                gateway = QString::fromLocal8Bit(host);
        }

        const double rxMbps = static_cast<double>(adapter->ReceiveLinkSpeed) / 1000000.0;
        const double txMbps = static_cast<double>(adapter->TransmitLinkSpeed) / 1000000.0;
        lines << QString("%1 | RX %2 Mbps | TX %3 Mbps | %4")
                     .arg(name)
                     .arg(QString::number(rxMbps, 'f', 0))
                     .arg(QString::number(txMbps, 'f', 0))
                     .arg(addresses.join(", ") + (gateway.isEmpty() ? QString{} : QString(" | Gateway %1").arg(gateway)));

        if (adapter->ReceiveLinkSpeed > 0 && adapter->ReceiveLinkSpeed < 100000000ULL)
            warnings << QString("Active adapter '%1' has a link speed below 100 Mbps.").arg(name);
    }

    if (lines.isEmpty())
        warnings << "No active physical network adapter was found.";
    return lines.isEmpty() ? "No network adapters could be inspected." : lines.join("\n");
}

bool pingHost(const QString &host, int attempts, QString &result, double &average, double &jitter, int &loss)
{
    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE)
        return false;

    const QByteArray hostBytes = host.toLatin1();
    IN_ADDR destinationAddress{};
    if (InetPtonA(AF_INET, hostBytes.constData(), &destinationAddress) != 1) {
        IcmpCloseHandle(icmp);
        return false;
    }
    const IPAddr destination = destinationAddress.S_un.S_addr;
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
            static_cast<WORD>(sizeof(payload) - 1),
            nullptr,
            replyBuffer,
            sizeof(replyBuffer),
            1000);

        if (replies > 0) {
            const auto *reply = reinterpret_cast<const ICMP_ECHO_REPLY *>(replyBuffer);
            const double rtt = static_cast<double>(reply->RoundTripTime);
            ++received;
            total += rtt;
            minimum = std::min(minimum, rtt);
            maximum = std::max(maximum, rtt);
        }
    }

    IcmpCloseHandle(icmp);
    loss = attempts > 0 ? static_cast<int>((attempts - received) * 100 / attempts) : 100;
    if (received == 0) {
        result = QString("%1: no replies (%2% packet loss)").arg(host).arg(loss);
        average = 0.0;
        jitter = 0.0;
        return true;
    }

    average = total / received;
    jitter = maximum - minimum;
    result = QString("%1: avg %2 ms | min %3 ms | max %4 ms | loss %5% | variation %6 ms")
                 .arg(host)
                 .arg(QString::number(average, 'f', 1))
                 .arg(QString::number(minimum, 'f', 1))
                 .arg(QString::number(maximum, 'f', 1))
                 .arg(loss)
                 .arg(QString::number(jitter, 'f', 1));
    if (average >= 100.0)
        warnings << QString("Internet latency to %1 is high (%2 ms average).").arg(host).arg(QString::number(average, 'f', 1));
    if (jitter >= 30.0)
        warnings << QString("Internet latency variation to %1 is high (%2 ms).").arg(host).arg(QString::number(jitter, 'f', 1));
    return true;
}

QString storageReport(QStringList &warnings)
{
    QStringList lines;
    const DWORD drives = GetLogicalDrives();
    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        const DWORD bit = 1u << (letter - L'A');
        if (!(drives & bit))
            continue;

        wchar_t root[] = {letter, L':', L'\\', L'\0'};
        if (GetDriveTypeW(root) != DRIVE_FIXED)
            continue;

        ULARGE_INTEGER freeBytes{}, totalBytes{}, availableBytes{};
        if (!GetDiskFreeSpaceExW(root, &availableBytes, &totalBytes, &freeBytes) || totalBytes.QuadPart == 0)
            continue;

        const double freePercent = static_cast<double>(availableBytes.QuadPart) * 100.0 /
                                   static_cast<double>(totalBytes.QuadPart);
        lines << QString("%1: %2% free")
                     .arg(QChar(letter))
                     .arg(QString::number(freePercent, 'f', 1));
        if (freePercent < 10.0)
            warnings << QString("Drive %1 has less than 10% free space.").arg(QChar(letter));
    }

    return lines.isEmpty() ? "No fixed drives could be inspected." : lines.join("\n");
}
#endif
}

DiagnosticReport runFullDiagnostic()
{
    DiagnosticReport report;
#ifdef Q_OS_WIN
    const SystemInfo system = collectSystemInfo();
    const TelemetrySnapshot telemetry = collectTelemetry();
    QString executablePath;
    const QString game = detectRunningGame(executablePath);
    QStringList warnings;

    report.summary = game.isEmpty()
        ? QStringLiteral("Diagnostic complete. No supported game process was detected.")
        : QString("Diagnostic complete. Detected game: %1").arg(game);

    QStringList sections;
    sections << "=== System ==="
             << system.toText();

    sections << "=== Running Game ==="
             << (game.isEmpty() ? QStringLiteral("No supported game detected.")
                                : QString("%1\nExecutable: %2").arg(game, executablePath));

    sections << "=== Live Performance ==="
             << QString("CPU: %1%\nGPU: %2%\nRAM: %3% used\nVRAM: %4% used\nGPU temperature: %5 C\nBottleneck: %6")
                    .arg(QString::number(telemetry.cpuUsagePercent, 'f', 1))
                    .arg(telemetry.gpuUsageAvailable ? QString::number(telemetry.gpuUsagePercent, 'f', 1) : QStringLiteral("unavailable"))
                    .arg(telemetry.totalMemoryBytes > 0
                             ? QString::number((1.0 - static_cast<double>(telemetry.availableMemoryBytes) /
                                                static_cast<double>(telemetry.totalMemoryBytes)) * 100.0, 'f', 1)
                             : QStringLiteral("unavailable"))
                    .arg(telemetry.gpuMemoryTotalBytes > 0
                             ? QString::number((static_cast<double>(telemetry.gpuMemoryUsedBytes) /
                                                static_cast<double>(telemetry.gpuMemoryTotalBytes)) * 100.0, 'f', 1)
                             : QStringLiteral("unavailable"))
                    .arg(telemetry.gpuTemperatureAvailable ? QString::number(telemetry.gpuTemperatureCelsius, 'f', 1) : QStringLiteral("unavailable"))
                    .arg(diagnoseBottleneck(telemetry));

    if (telemetry.perCoreUsageAvailable) {
        double hottestCore = 0.0;
        int hottestIndex = -1;
        for (int i = 0; i < telemetry.perCoreUsagePercent.size(); ++i) {
            if (telemetry.perCoreUsagePercent.at(i) > hottestCore) {
                hottestCore = telemetry.perCoreUsagePercent.at(i);
                hottestIndex = i;
            }
        }
        sections << QString("CPU cores: %1 logical cores sampled | hottest core %2: %3%")
                        .arg(telemetry.perCoreUsagePercent.size())
                        .arg(hottestIndex)
                        .arg(QString::number(hottestCore, 'f', 1));
    }

    QString gatewayResult;
    double gatewayAverage = 0.0;
    double gatewayJitter = 0.0;
    int gatewayLoss = 0;
    const bool gatewayOk = pingHost(QStringLiteral("192.168.1.1"), 2, gatewayResult, gatewayAverage, gatewayJitter, gatewayLoss);
    Q_UNUSED(gatewayOk);
    sections << "=== Network ==="
             << networkAdapterReport(warnings);

    QString internetResult;
    double internetAverage = 0.0;
    double internetJitter = 0.0;
    int internetLoss = 0;
    const bool internetOk = pingHost(QStringLiteral("1.1.1.1"), 4, internetResult, internetAverage, internetJitter, internetLoss);
    Q_UNUSED(internetOk);
    sections << QString("Gateway test: %1\nInternet test: %2")
                    .arg(gatewayResult.isEmpty() ? QStringLiteral("unavailable") : gatewayResult)
                    .arg(internetResult.isEmpty() ? QStringLiteral("unavailable") : internetResult);

    sections << "=== Storage ==="
             << storageReport(warnings);

    if (!warnings.isEmpty())
        sections << "=== Warnings ===" << warnings.join("\n");
    else
        sections << "=== Warnings ===" << "No high-confidence warnings were detected.";

    report.details = sections.join("\n\n");
#else
    report.summary = QStringLiteral("Full diagnostics are currently implemented for Windows only.");
    report.details = report.summary;
#endif
    return report;
}