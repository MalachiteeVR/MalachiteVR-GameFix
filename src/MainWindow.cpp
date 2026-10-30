#include "MainWindow.h"
#include "SystemInfo.h"
#include "Telemetry.h"

#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("MalachiteVR GameFix");
    resize(1000, 780);
    setMinimumSize(860, 680);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);

    auto *title = new QLabel("MalachiteVR GameFix", central);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *subtitle = new QLabel("Find out why your games are running poorly.", central);
    subtitle->setObjectName("subtitle");

    auto makeCard = [central](const QString &heading, QLabel *&value) {
        auto *box = new QGroupBox(heading, central);
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(16, 14, 16, 16);
        value = new QLabel("Detecting...", box);
        value->setWordWrap(true);
        QFont font = value->font();
        font.setPointSize(12);
        font.setBold(true);
        value->setFont(font);
        layout->addWidget(value);
        return box;
    };

    auto *systemGrid = new QGridLayout;
    systemGrid->setSpacing(12);
    systemGrid->addWidget(makeCard("CPU", cpuValue_), 0, 0);
    systemGrid->addWidget(makeCard("GPU", gpuValue_), 0, 1);
    systemGrid->addWidget(makeCard("Memory", memoryValue_), 1, 0);
    systemGrid->addWidget(makeCard("Operating System", osValue_), 1, 1);
    systemGrid->addWidget(makeCard("Architecture", architectureValue_), 2, 0);
    systemGrid->addWidget(makeCard("GameFix Status", statusValue_), 2, 1);

    auto *telemetryTitle = new QLabel("Live Performance", central);
    QFont telemetryFont = telemetryTitle->font();
    telemetryFont.setPointSize(16);
    telemetryFont.setBold(true);
    telemetryTitle->setFont(telemetryFont);

    auto *telemetrySubtitle = new QLabel(
        "Updated every second. Values are read-only and are not sent anywhere.", central);
    telemetrySubtitle->setObjectName("subtitle");

    auto *telemetryGrid = new QGridLayout;
    telemetryGrid->setSpacing(12);
    telemetryGrid->addWidget(makeCard("CPU Usage", cpuUsageValue_), 0, 0);
    telemetryGrid->addWidget(makeCard("GPU Usage", gpuUsageValue_), 0, 1);
    telemetryGrid->addWidget(makeCard("RAM Usage", ramUsageValue_), 1, 0);
    telemetryGrid->addWidget(makeCard("VRAM Usage", vramUsageValue_), 1, 1);
    telemetryGrid->addWidget(makeCard("GPU Temperature", gpuTemperatureValue_), 2, 0);
    telemetryGrid->addWidget(makeCard("Bottleneck Analysis", diagnosisValue_), 2, 1);

    auto *buttonRow = new QHBoxLayout;
    auto *refreshButton = new QPushButton("Refresh System Information", central);
    auto *diagnosticButton = new QPushButton("Run Full Diagnostic", central);
    diagnosticButton->setEnabled(false);
    buttonRow->addWidget(refreshButton);
    buttonRow->addWidget(diagnosticButton);

    auto *footer = new QLabel("v0.2.0  •  Windows  •  Qt 6  •  C++20  •  Live telemetry", central);
    footer->setObjectName("footer");

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addLayout(systemGrid);
    root->addWidget(telemetryTitle);
    root->addWidget(telemetrySubtitle);
    root->addLayout(telemetryGrid);
    root->addStretch();
    root->addLayout(buttonRow);
    root->addWidget(footer);

    setCentralWidget(central);

    setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #0b0f0d;
            color: #e8f5ec;
        }
        QLabel#subtitle, QLabel#footer {
            color: #91a49a;
        }
        QGroupBox {
            border: 1px solid #26352c;
            border-radius: 10px;
            margin-top: 12px;
            padding-top: 12px;
            background: #101712;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
            color: #65d98a;
        }
        QPushButton {
            background: #173d25;
            border: 1px solid #2f7547;
            border-radius: 7px;
            padding: 10px 16px;
            color: #ecfff1;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #205a34;
        }
        QPushButton:disabled {
            background: #151b17;
            color: #65736b;
            border-color: #28312b;
        }
    )");

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshSystemInfo);

    telemetryTimer_ = new QTimer(this);
    telemetryTimer_->setInterval(1000);
    connect(telemetryTimer_, &QTimer::timeout, this, &MainWindow::refreshTelemetry);

    refreshSystemInfo();
    refreshTelemetry();
    telemetryTimer_->start();
}

void MainWindow::refreshSystemInfo()
{
    const SystemInfo info = collectSystemInfo();

    cpuValue_->setText(QString("%1\n%2 logical processors")
                           .arg(info.cpu)
                           .arg(info.logicalProcessors));
    gpuValue_->setText(info.graphics);
    memoryValue_->setText(QString("%1 total\n%2 available")
                              .arg(formatBytes(info.totalMemoryBytes))
                              .arg(formatBytes(info.availableMemoryBytes)));
    osValue_->setText(info.operatingSystem);
    architectureValue_->setText(info.architecture);
    statusValue_->setText("System information collected");
}

void MainWindow::refreshTelemetry()
{
    const TelemetrySnapshot telemetry = collectTelemetry();

    cpuUsageValue_->setText(QString("%1%")
                                .arg(QString::number(telemetry.cpuUsagePercent, 'f', 0)));

    if (telemetry.gpuUsageAvailable) {
        gpuUsageValue_->setText(QString("%1%")
                                    .arg(QString::number(telemetry.gpuUsagePercent, 'f', 0)));
    } else {
        gpuUsageValue_->setText("Sampling...");
    }

    const quint64 usedRam = telemetry.totalMemoryBytes >= telemetry.availableMemoryBytes
                                ? telemetry.totalMemoryBytes - telemetry.availableMemoryBytes
                                : 0;
    ramUsageValue_->setText(QString("%1 / %2")
                                .arg(formatBytes(usedRam))
                                .arg(formatBytes(telemetry.totalMemoryBytes)));

    if (telemetry.gpuMemoryTotalBytes > 0) {
        vramUsageValue_->setText(QString("%1 / %2")
                                     .arg(formatBytes(telemetry.gpuMemoryUsedBytes))
                                     .arg(formatBytes(telemetry.gpuMemoryTotalBytes)));
    } else {
        vramUsageValue_->setText("Unavailable");
    }

    if (telemetry.gpuTemperatureAvailable) {
        gpuTemperatureValue_->setText(QString("%1 °C")
                                          .arg(QString::number(telemetry.gpuTemperatureCelsius, 'f', 0)));
    } else {
        gpuTemperatureValue_->setText("Unavailable");
    }

    diagnosisValue_->setText(diagnoseBottleneck(telemetry));
}
