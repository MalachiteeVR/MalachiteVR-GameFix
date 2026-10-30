#include "Fixes.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winreg.h>
#endif

namespace
{
#ifdef Q_OS_WIN
QSettings appSettings()
{
    return QSettings("MalachiteeVR", "MalachiteVR GameFix");
}

FixResult result(const QString &title, bool success, const QString &message, bool restart = false)
{
    return {success, restart, title, message};
}

bool runPowerCfg(const QStringList &arguments, QString *output = nullptr)
{
    QProcess process;
    process.start("powercfg.exe", arguments);
    if (!process.waitForFinished(5000)) {
        process.kill();
        return false;
    }

    if (output)
        *output = QString::fromLocal8Bit(process.readAllStandardOutput() + process.readAllStandardError());
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString activePowerScheme()
{
    QString output;
    if (!runPowerCfg({"/getactivescheme"}, &output))
        return {};

    const QRegularExpression expression(QStringLiteral("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"));
    const QRegularExpressionMatch match = expression.match(output);
    return match.hasMatch() ? match.captured(0) : QString{};
}

bool isProcessElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool success = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
                         elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return success;
}

bool writeDword(HKEY root, const wchar_t *subKey, const wchar_t *valueName, DWORD value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG status = RegSetValueExW(key, valueName, 0, REG_DWORD,
                                       reinterpret_cast<const BYTE *>(&value), sizeof(value));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

bool deleteValue(HKEY root, const wchar_t *subKey, const wchar_t *valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return false;
    const LONG status = RegDeleteValueW(key, valueName);
    RegCloseKey(key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}
#endif
}

FixResult FixManager::enableGameMode()
{
#ifdef Q_OS_WIN
    QSettings settings = appSettings();
    if (!settings.contains("backup/gameModeExists")) {
        QSettings gameBar("HKEY_CURRENT_USER\\Software\\Microsoft\\GameBar", QSettings::NativeFormat);
        settings.setValue("backup/gameModeExists", gameBar.contains("AutoGameModeEnabled"));
        if (gameBar.contains("AutoGameModeEnabled"))
            settings.setValue("backup/gameModeValue", gameBar.value("AutoGameModeEnabled"));
        settings.sync();
    }

    QSettings gameBar("HKEY_CURRENT_USER\\Software\\Microsoft\\GameBar", QSettings::NativeFormat);
    gameBar.setValue("AutoGameModeEnabled", 1);
    gameBar.sync();
    return result("Game Mode", gameBar.status() == QSettings::NoError,
                  "Windows Game Mode was enabled for the current user.");
#else
    return result("Game Mode", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::restoreGameMode()
{
#ifdef Q_OS_WIN
    QSettings settings = appSettings();
    if (!settings.contains("backup/gameModeExists"))
        return result("Restore Game Mode", false, "GameFix has no saved Game Mode change to restore.");

    QSettings gameBar("HKEY_CURRENT_USER\\Software\\Microsoft\\GameBar", QSettings::NativeFormat);
    const bool existed = settings.value("backup/gameModeExists").toBool();
    if (existed)
        gameBar.setValue("AutoGameModeEnabled", settings.value("backup/gameModeValue"));
    else
        gameBar.remove("AutoGameModeEnabled");
    gameBar.sync();
    settings.remove("backup/gameModeExists");
    settings.remove("backup/gameModeValue");
    settings.sync();
    return result("Restore Game Mode", gameBar.status() == QSettings::NoError,
                  "The previous Game Mode registry value was restored.");
#else
    return result("Restore Game Mode", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::useHighPerformancePowerPlan()
{
#ifdef Q_OS_WIN
    QSettings settings = appSettings();
    if (!settings.contains("backup/powerScheme")) {
        const QString current = activePowerScheme();
        if (current.isEmpty())
            return result("High Performance Power Plan", false, "Could not determine the current power plan.");
        settings.setValue("backup/powerScheme", current);
        settings.sync();
    }

    const bool success = runPowerCfg({"/setactive", "SCHEME_MIN"});
    return result("High Performance Power Plan", success,
                  success ? "The High Performance power plan is now active. The previous plan is saved for restoration."
                          : "Windows could not activate the High Performance power plan.");
#else
    return result("High Performance Power Plan", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::restorePowerPlan()
{
#ifdef Q_OS_WIN
    QSettings settings = appSettings();
    const QString previous = settings.value("backup/powerScheme").toString();
    if (previous.isEmpty())
        return result("Restore Power Plan", false, "GameFix has no saved power plan to restore.");

    const bool success = runPowerCfg({"/setactive", previous});
    if (success) {
        settings.remove("backup/powerScheme");
        settings.sync();
    }
    return result("Restore Power Plan", success,
                  success ? "The power plan active before GameFix was restored."
                          : "Windows could not restore the previous power plan.");
#else
    return result("Restore Power Plan", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::disableFullscreenOptimizations(const QString &executablePath)
{
#ifdef Q_OS_WIN
    if (executablePath.isEmpty())
        return result("Fullscreen Optimizations", false, "No game executable path was identified.");

    QSettings layers("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
                     QSettings::NativeFormat);
    QSettings settings = appSettings();
    const QString key = QStringLiteral("backup/fullscreen/") + executablePath;

    if (!settings.contains(key + "/exists")) {
        settings.setValue(key + "/exists", layers.contains(executablePath));
        if (layers.contains(executablePath))
            settings.setValue(key + "/value", layers.value(executablePath));
        settings.sync();
    }

    layers.setValue(executablePath, QStringLiteral("~ DISABLEDXMAXIMIZEDWINDOWEDMODE"));
    layers.sync();
    return result("Fullscreen Optimizations", layers.status() == QSettings::NoError,
                  "Fullscreen Optimizations were disabled for the detected game. Relaunch the game for the compatibility flag to take effect.");
#else
    Q_UNUSED(executablePath);
    return result("Fullscreen Optimizations", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::restoreFullscreenOptimizations()
{
#ifdef Q_OS_WIN
    QSettings settings = appSettings();
    QSettings layers("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
                     QSettings::NativeFormat);

    const QStringList keys = settings.allKeys();
    int restored = 0;
    for (const QString &key : keys) {
        if (!key.startsWith("backup/fullscreen/") || !key.endsWith("/exists"))
            continue;

        const QString base = key.left(key.size() - QStringLiteral("/exists").size());
        const QString path = base.mid(QStringLiteral("backup/fullscreen/").size());
        const bool existed = settings.value(key).toBool();
        if (existed)
            layers.setValue(path, settings.value(base + "/value"));
        else
            layers.remove(path);
        settings.remove(base + "/exists");
        settings.remove(base + "/value");
        ++restored;
    }

    layers.sync();
    settings.sync();
    return result("Restore Fullscreen Optimizations", true,
                  restored > 0 ? QString("Restored %1 saved game compatibility setting(s).").arg(restored)
                               : "There were no saved fullscreen optimization changes to restore.");
#else
    return result("Restore Fullscreen Optimizations", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::enableHardwareAcceleratedGpuScheduling()
{
#ifdef Q_OS_WIN
    if (!isProcessElevated())
        return result("Hardware-Accelerated GPU Scheduling", false,
                      "This change writes to HKLM and requires GameFix to be run as administrator.");

    QSettings settings = appSettings();
    if (!settings.contains("backup/hagsExists")) {
        QSettings graphicsDrivers("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                                  QSettings::NativeFormat);
        settings.setValue("backup/hagsExists", graphicsDrivers.contains("HwSchMode"));
        if (graphicsDrivers.contains("HwSchMode"))
            settings.setValue("backup/hagsValue", graphicsDrivers.value("HwSchMode"));
        settings.sync();
    }

    const bool success = writeDword(HKEY_LOCAL_MACHINE,
                                    L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                                    L"HwSchMode", 2);
    return result("Hardware-Accelerated GPU Scheduling", success,
                  success ? "Hardware-Accelerated GPU Scheduling was enabled. Windows requires a restart for the change to take effect."
                          : "Windows rejected the Hardware-Accelerated GPU Scheduling change.", true);
#else
    return result("Hardware-Accelerated GPU Scheduling", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::disableHardwareAcceleratedGpuScheduling()
{
#ifdef Q_OS_WIN
    if (!isProcessElevated())
        return result("Hardware-Accelerated GPU Scheduling", false,
                      "This change writes to HKLM and requires GameFix to be run as administrator.");

    QSettings settings = appSettings();
    if (!settings.contains("backup/hagsExists")) {
        QSettings graphicsDrivers("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                                  QSettings::NativeFormat);
        settings.setValue("backup/hagsExists", graphicsDrivers.contains("HwSchMode"));
        if (graphicsDrivers.contains("HwSchMode"))
            settings.setValue("backup/hagsValue", graphicsDrivers.value("HwSchMode"));
        settings.sync();
    }

    const bool success = writeDword(HKEY_LOCAL_MACHINE,
                                    L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                                    L"HwSchMode", 1);
    return result("Hardware-Accelerated GPU Scheduling", success,
                  success ? "Hardware-Accelerated GPU Scheduling was disabled. Windows requires a restart for the change to take effect."
                          : "Windows rejected the Hardware-Accelerated GPU Scheduling change.", true);
#else
    return result("Hardware-Accelerated GPU Scheduling", false, "This fix is only available on Windows.");
#endif
}

FixResult FixManager::restoreHardwareAcceleratedGpuScheduling()
{
#ifdef Q_OS_WIN
    if (!isProcessElevated())
        return result("Restore Hardware-Accelerated GPU Scheduling", false,
                      "This change writes to HKLM and requires GameFix to be run as administrator.");

    QSettings settings = appSettings();
    if (!settings.contains("backup/hagsExists"))
        return result("Restore Hardware-Accelerated GPU Scheduling", false,
                      "GameFix has no saved HAGS change to restore.");

    const bool existed = settings.value("backup/hagsExists").toBool();
    bool success = false;
    if (existed) {
        const DWORD value = settings.value("backup/hagsValue").toUInt();
        success = writeDword(HKEY_LOCAL_MACHINE,
                             L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                             L"HwSchMode", value);
    } else {
        success = deleteValue(HKEY_LOCAL_MACHINE,
                              L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
                              L"HwSchMode");
    }

    if (success) {
        settings.remove("backup/hagsExists");
        settings.remove("backup/hagsValue");
        settings.sync();
    }
    return result("Restore Hardware-Accelerated GPU Scheduling", success,
                  success ? "The previous Hardware-Accelerated GPU Scheduling setting was restored. Restart Windows to apply it."
                          : "Windows could not restore the previous Hardware-Accelerated GPU Scheduling setting.", true);
#else
    return result("Restore Hardware-Accelerated GPU Scheduling", false, "This fix is only available on Windows.");
#endif
}
