#include "PerformanceHistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

namespace
{
QString historyPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(base);
    return QDir(base).filePath("performance-history.json");
}

QJsonObject sampleToJson(const HistorySample &sample)
{
    QJsonObject object;
    object["timestamp"] = sample.timestamp.toString(Qt::ISODate);
    object["game"] = sample.game;
    object["cpu"] = sample.cpuUsagePercent;
    object["gpu"] = sample.gpuUsagePercent;
    object["ram"] = sample.ramUsagePercent;
    object["vram"] = sample.vramUsagePercent;
    object["gpuTemp"] = sample.gpuTemperatureCelsius;
    object["cpuTemp"] = sample.cpuTemperatureCelsius;
    return object;
}

HistorySample sampleFromJson(const QJsonObject &object)
{
    HistorySample sample;
    sample.timestamp = QDateTime::fromString(object.value("timestamp").toString(), Qt::ISODate);
    sample.game = object.value("game").toString();
    sample.cpuUsagePercent = object.value("cpu").toDouble();
    sample.gpuUsagePercent = object.value("gpu").toDouble();
    sample.ramUsagePercent = object.value("ram").toDouble();
    sample.vramUsagePercent = object.value("vram").toDouble();
    sample.gpuTemperatureCelsius = object.value("gpuTemp").toDouble();
    sample.cpuTemperatureCelsius = object.value("cpuTemp").toDouble();
    return sample;
}
}

bool PerformanceHistory::saveSample(const HistorySample &sample)
{
    QJsonObject root = loadRoot();
    const QString gameName = normalizedGame(sample.game);
    QJsonArray samples = root.value(gameName).toArray();

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
    const QJsonObject root = loadRoot();
    return root.keys();
}

bool PerformanceHistory::clearGame(const QString &game)
{
    QJsonObject root = loadRoot();
    root.remove(normalizedGame(game));
    return saveRoot(root);
}

QJsonObject PerformanceHistory::loadRoot() const
{
    QFile file(historyPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

bool PerformanceHistory::saveRoot(const QJsonObject &root) const
{
    QFile file(historyPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.error() == QFileDevice::NoError;
}

QString PerformanceHistory::normalizedGame(const QString &game)
{
    const QString trimmed = game.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("Unknown Game") : trimmed;
}
