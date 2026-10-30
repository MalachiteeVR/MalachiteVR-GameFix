#pragma once

#include <QMainWindow>

class QLabel;
class QTimer;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void refreshSystemInfo();
    void refreshTelemetry();

    QLabel *cpuValue_ = nullptr;
    QLabel *gpuValue_ = nullptr;
    QLabel *memoryValue_ = nullptr;
    QLabel *osValue_ = nullptr;
    QLabel *architectureValue_ = nullptr;
    QLabel *statusValue_ = nullptr;
    QLabel *cpuUsageValue_ = nullptr;
    QLabel *gpuUsageValue_ = nullptr;
    QLabel *ramUsageValue_ = nullptr;
    QLabel *vramUsageValue_ = nullptr;
    QLabel *gpuTemperatureValue_ = nullptr;
    QLabel *diagnosisValue_ = nullptr;
    QTimer *telemetryTimer_ = nullptr;
};
