#pragma once

#include <QMainWindow>

class QLabel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void refreshSystemInfo();

    QLabel *cpuValue_ = nullptr;
    QLabel *gpuValue_ = nullptr;
    QLabel *memoryValue_ = nullptr;
    QLabel *osValue_ = nullptr;
    QLabel *architectureValue_ = nullptr;
    QLabel *statusValue_ = nullptr;
};
