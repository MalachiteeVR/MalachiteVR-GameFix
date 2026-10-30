#pragma once

#include <QString>

struct SystemInfo
{
    QString operatingSystem;
    QString architecture;
    QString cpu;
    int logicalProcessors = 0;
    quint64 totalMemoryBytes = 0;
    quint64 availableMemoryBytes = 0;
    QString graphics;
};

SystemInfo collectSystemInfo();
QString formatBytes(quint64 bytes);
