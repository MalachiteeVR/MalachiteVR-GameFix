#pragma once

#include "Telemetry.h"

#include <QVector>
#include <QString>

struct HistorySample
{
    QString timestamp;
    double cpuUsagePercent = 0.0;
    double gpuUsagePercent = 0.0;
    double ramUsagePercent = 0.0;
    double vramUsagePercent = 0.0;
    double gpuTemperatureCelsius = 0.0;
    bool gpuTemperatureAvailable = false;
};

class PerformanceHistory
{
public:
    static bool appendSample(const QString &game, const TelemetrySnapshot &telemetry);
    static QVector<HistorySample> loadSamples(const QString &game, int limit = 600);
    static QStringList games();
    static bool clearGame(const QString &game);
    static QString storageFilePath();
    static QString summarize(const QVector<HistorySample> &samples);
};
