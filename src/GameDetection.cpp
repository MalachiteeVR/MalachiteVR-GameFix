#include "Diagnostic.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <TlHelp32.h>
#endif

#include <QStringList>

DetectedGame detectRunningGame()
{
    DetectedGame result;

#ifdef Q_OS_WIN
    static const QStringList knownGames = {
        "vrchat.exe", "beatsaber.exe", "minecraft.exe", "minecraftlauncher.exe",
        "eldenring.exe", "fortniteclient-win64-shipping.exe", "cs2.exe", "csgo.exe",
        "valorant-win64-shipping.exe", "helldivers2.exe", "palworld-win64-shipping.exe",
        "r5apex.exe", "starfield.exe", "cyberpunk2077.exe", "gta5.exe",
        "rocketleague.exe", "overwatch.exe"
    };

    auto fillResult = [](DetectedGame &game, DWORD pid, const QString &name) {
        game.name = name;
        game.processName = name;
        game.processId = static_cast<quint64>(pid);
        game.running = true;

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
            return;

        wchar_t path[32768]{};
        DWORD length = static_cast<DWORD>(std::size(path));
        if (QueryFullProcessImageNameW(process, 0, path, &length))
            game.executablePath = QString::fromWCharArray(path, static_cast<int>(length));
        CloseHandle(process);
    };

    HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foregroundWindow)
        GetWindowThreadProcessId(foregroundWindow, &foregroundPid);

    if (foregroundPid != 0) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, foregroundPid);
        if (process) {
            wchar_t path[32768]{};
            DWORD length = static_cast<DWORD>(std::size(path));
            if (QueryFullProcessImageNameW(process, 0, path, &length)) {
                const QString executablePath = QString::fromWCharArray(path, static_cast<int>(length));
                const QString processName = executablePath.section('\\', -1).toLower();
                if (knownGames.contains(processName)) {
                    fillResult(result, foregroundPid, processName);
                    return result;
                }
            }
            CloseHandle(process);
        }
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return result;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            const QString processName = QString::fromWCharArray(entry.szExeFile).toLower();
            if (knownGames.contains(processName)) {
                fillResult(result, entry.th32ProcessID, processName);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
#else
    Q_UNUSED(result);
#endif

    return result;
}
