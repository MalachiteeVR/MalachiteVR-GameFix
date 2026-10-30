#pragma once

#include <QString>

struct FixResult
{
    bool success = false;
    bool requiresRestart = false;
    QString title;
    QString message;
};

class FixManager
{
public:
    static FixResult enableGameMode();
    static FixResult restoreGameMode();
    static FixResult useHighPerformancePowerPlan();
    static FixResult restorePowerPlan();
    static FixResult disableFullscreenOptimizations(const QString &executablePath);
    static FixResult restoreFullscreenOptimizations();
    static FixResult enableHardwareAcceleratedGpuScheduling();
    static FixResult disableHardwareAcceleratedGpuScheduling();
    static FixResult restoreHardwareAcceleratedGpuScheduling();
};
