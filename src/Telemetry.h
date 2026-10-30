#pragma once

#include <QString>
#include <QtGlobal>

struct TelemetrySnapshot
{
    double cpuUsagePercent = 0.0;
    double gpuUsagePercent = 0.0;
    quint64 totalMemoryBytes = 0;
    quint64 availableMemoryBytes = 0;
    quint64 gpuMemoryUsedBytes = 0;
    quint64 gpuMemoryTotalBytes = 0;
    double gpuTemperatureCelsius = 0.0;
    bool gpuTemperatureAvailable = false;
    bool gpuUsageAvailable = false;
};

TelemetrySnapshot collectTelemetry();
QString diagnoseBottleneck(const TelemetrySnapshot &telemetry);
