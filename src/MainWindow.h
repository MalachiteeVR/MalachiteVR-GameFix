#pragma once

#include <QMainWindow>

#include "Diagnostic.h"

class QLabel;
class QPushButton;
class QTimer;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void refreshSystemInfo();
    void refreshTelemetry();
    void showHistoryDialog();
    void showFixesDialog();

    QLabel *cpuValue_ = nullptr;
    QLabel *gpuValue_ = nullptr;
    QLabel *memoryValue_ = nullptr;
    QLabel *osValue_ = nullptr;
    QLabel *architectureValue_ = nullptr;
    QLabel *statusValue_ = nullptr;
    QLabel *gameValue_ = nullptr;
    QLabel *cpuUsageValue_ = nullptr;
    QLabel *gpuUsageValue_ = nullptr;
    QLabel *ramUsageValue_ = nullptr;
    QLabel *vramUsageValue_ = nullptr;
    QLabel *gpuTemperatureValue_ = nullptr;
    QLabel *perCoreValue_ = nullptr;
    QLabel *diagnosisValue_ = nullptr;
    QLabel *captureStatusValue_ = nullptr;
    QPushButton *captureButton_ = nullptr;
    QTimer *telemetryTimer_ = nullptr;
    bool captureActive_ = false;
    QString captureGameName_;
    DetectedGame detectedGame_;
};
