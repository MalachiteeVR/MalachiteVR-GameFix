#include "PerformanceHistory.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

namespace
{
QString normalizedGame(const QString &game)
{
    const QString trimmed = game.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("Unknown Game") : trimmed;
}

QJsonObject sampleToJson(const HistorySample &sample)
{
    QJsonObject object;
    object["timestamp"] = sample.timestamp;
    object["cpu"] = sample.cpuUsagePercent;
    object["gpu"] = sample.gpuUsagePercent;
    object["ram"] = sample.ramUsagePercent;
    object["vram"] = sample.vramUsagePercent;
    object["gpuTemp"] = sample.gpuTemperatureCelsius;
    object["gpuTempAvailable"] = sample.gpuTemperatureAvailable;
    return object;
}

HistorySample sampleFromJson(const QJsonObject &object)
{
    HistorySample sample;
    sample.timestamp = object.value("timestamp").toString();
    sample.cpuUsagePercent = object.value("cpu").toDouble();
    sample.gpuUsagePercent = object.value("gpu").toDouble();
    sample.ramUsagePercent = object.value("ram").toDouble();
    sample.vramUsagePercent = object.value("vram").toDouble();
    sample.gpuTemperatureCelsius = object.value("gpuTemp").toDouble();
    sample.gpuTemperatureAvailable = object.value("gpuTempAvailable").toBool();
    return sample;
}

QJsonObject loadRoot()
{
    QFile file(PerformanceHistory::storageFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

bool saveRoot(const QJsonObject &root)
{
    const QString path = PerformanceHistory::storageFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}
}

QString PerformanceHistory::storageFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base).filePath("performance-history.json");
}

bool PerformanceHistory::appendSample(const QString &game, const TelemetrySnapshot &telemetry)
{
    const QString gameName = normalizedGame(game);
    QJsonObject root = loadRoot();
    QJsonArray samples = root.value(gameName).toArray();

    HistorySample sample;
    sample.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    sample.cpuUsagePercent = telemetry.cpuUsagePercent;
    sample.gpuUsagePercent = telemetry.gpuUsagePercent;
    sample.ramUsagePercent = telemetry.totalMemoryBytes > 0
                                 ? (1.0 - static_cast<double>(telemetry.availableMemoryBytes) /
                                           static_cast<double>(telemetry.totalMemoryBytes)) * 100.0
                                 : 0.0;
    sample.vramUsagePercent = telemetry.gpuMemoryTotalBytes > 0
                                  ? static_cast<double>(telemetry.gpuMemoryUsedBytes) /
                                        static_cast<double>(telemetry.gpuMemoryTotalBytes) * 100.0
                                  : 0.0;
    sample.gpuTemperatureCelsius = telemetry.gpuTemperatureCelsius;
    sample.gpuTemperatureAvailable = telemetry.gpuTemperatureAvailable;

    samples.append(sampleToJson(sample));

    constexpr int maxSamplesPerGame = 7200;
    while (samples.size() > maxSamplesPerGame)
        samples.removeFirst();

    root[gameName] = samples;
    return saveRoot(root);
}

QVector<HistorySample> PerformanceHistory::loadSamples(const QString &game, int limit)
{
    QVector<HistorySample> result;
    const QJsonArray samples = loadRoot().value(normalizedGame(game)).toArray();
    const int requested = std::max(1, limit);
    const int sampleCount = static_cast<int>(samples.size());
    const int start = std::max(0, sampleCount - requested);

    for (int i = start; i < sampleCount; ++i) {
        if (samples.at(i).isObject())
            result.append(sampleFromJson(samples.at(i).toObject()));
    }
    return result;
}

QStringList PerformanceHistory::games()
{
    return loadRoot().keys();
}

bool PerformanceHistory::clearGame(const QString &game)
{
    QJsonObject root = loadRoot();
    root.remove(normalizedGame(game));
    return saveRoot(root);
}

QString PerformanceHistory::summarize(const QVector<HistorySample> &samples)
{
    if (samples.isEmpty())
        return "No captured samples for this game.";

    double cpu = 0.0;
    double gpu = 0.0;
    double ram = 0.0;
    double vram = 0.0;
    double gpuTemp = 0.0;
    int tempSamples = 0;

    for (const HistorySample &sample : samples) {
        cpu += sample.cpuUsagePercent;
        gpu += sample.gpuUsagePercent;
        ram += sample.ramUsagePercent;
        vram += sample.vramUsagePercent;
        if (sample.gpuTemperatureAvailable) {
            gpuTemp += sample.gpuTemperatureCelsius;
            ++tempSamples;
        }
    }

    const double count = static_cast<double>(samples.size());
    QString result = QString("%1 samples | CPU avg %2% | GPU avg %3% | RAM avg %4% | VRAM avg %5%")
                         .arg(samples.size())
                         .arg(QString::number(cpu / count, 'f', 1))
                         .arg(QString::number(gpu / count, 'f', 1))
                         .arg(QString::number(ram / count, 'f', 1))
                         .arg(QString::number(vram / count, 'f', 1));

    if (tempSamples > 0)
        result += QString(" | GPU temp avg %1 °C")
                      .arg(QString::number(gpuTemp / tempSamples, 'f', 1));

    return result;
}