#pragma once

#include <QString>
#include <QtGlobal>

struct DetectedGame
{
    QString name;
    QString processName;
    QString executablePath;
    quint64 processId = 0;
    bool running = false;
};

struct DiagnosticReport
{
    QString summary;
    QString details;
    DetectedGame game;
};

DetectedGame detectRunningGame();
DiagnosticReport runFullDiagnostic();
