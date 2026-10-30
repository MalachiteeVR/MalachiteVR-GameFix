#include "MainWindow.h"
#include "Diagnostic.h"
#include "SystemInfo.h"
#include "Telemetry.h"

#include <QApplication>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("MalachiteVR GameFix");
    resize(1000, 780);
    setMinimumSize(720, 520);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *central = new QWidget;
    central->setMinimumWidth(680);
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
    subtitle->setWordWrap(true);

    auto makeCard = [central](const QString &heading, QLabel *&value) {
        auto *box = new QGroupBox(heading, central);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(8);

        value = new QLabel("Detecting...", box);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        value->setMinimumWidth(0);
        QFont font = value->font();
        font.setPointSize(12);
        font.setBold(true);
        value->setFont(font);
        layout->addWidget(value);
        return box;
    };

    auto *systemGrid = new QGridLayout;
    systemGrid->setSpacing(12);
    systemGrid->setColumnStretch(0, 1);
    systemGrid->setColumnStretch(1, 1);
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
    telemetrySubtitle->setWordWrap(true);

    auto *telemetryGrid = new QGridLayout;
    telemetryGrid->setSpacing(12);
    telemetryGrid->setColumnStretch(0, 1);
    telemetryGrid->setColumnStretch(1, 1);
    telemetryGrid->addWidget(makeCard("CPU Usage", cpuUsageValue_), 0, 0);
    telemetryGrid->addWidget(makeCard("GPU Usage", gpuUsageValue_), 0, 1);
    telemetryGrid->addWidget(makeCard("RAM Usage", ramUsageValue_), 1, 0);
    telemetryGrid->addWidget(makeCard("VRAM Usage", vramUsageValue_), 1, 1);
    telemetryGrid->addWidget(makeCard("GPU Temperature", gpuTemperatureValue_), 2, 0);
    telemetryGrid->addWidget(makeCard("Bottleneck Analysis", diagnosisValue_), 2, 1);

    auto *buttonRow = new QHBoxLayout;
    auto *refreshButton = new QPushButton("Refresh System Information", central);
    auto *diagnosticButton = new QPushButton("Run Full Diagnostic", central);
    refreshButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    diagnosticButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttonRow->addWidget(refreshButton);
    buttonRow->addWidget(diagnosticButton);

    auto *footer = new QLabel("v0.3.0  •  Windows  •  Qt 6  •  C++20  •  Live telemetry + diagnostics", central);
    footer->setObjectName("footer");
    footer->setWordWrap(true);

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addLayout(systemGrid);
    root->addWidget(telemetryTitle);
    root->addWidget(telemetrySubtitle);
    root->addLayout(telemetryGrid);
    root->addStretch(1);
    root->addLayout(buttonRow);
    root->addWidget(footer);

    scrollArea->setWidget(central);
    setCentralWidget(scrollArea);

    setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #0b0f0d;
            color: #e8f5ec;
        }
        QScrollArea {
            border: none;
            background: #0b0f0d;
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
        QPlainTextEdit {
            background: #080c0a;
            border: 1px solid #26352c;
            border-radius: 8px;
            padding: 10px;
            color: #e8f5ec;
            selection-background-color: #205a34;
        }
    )");

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshSystemInfo);
    connect(diagnosticButton, &QPushButton::clicked, this, [this, diagnosticButton]() {
        diagnosticButton->setEnabled(false);
        diagnosticButton->setText("Running Diagnostic...");
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents();

        const DiagnosticReport report = runFullDiagnostic();

        QApplication::restoreOverrideCursor();
        diagnosticButton->setText("Run Full Diagnostic");
        diagnosticButton->setEnabled(true);

        QDialog dialog(this);
        dialog.setWindowTitle("MalachiteVR GameFix - Diagnostic Report");
        dialog.resize(860, 680);
        dialog.setMinimumSize(640, 480);

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto *summary = new QLabel(report.summary, &dialog);
        summary->setWordWrap(true);
        QFont summaryFont = summary->font();
        summaryFont.setPointSize(13);
        summaryFont.setBold(true);
        summary->setFont(summaryFont);

        auto *details = new QPlainTextEdit(&dialog);
        details->setReadOnly(true);
        details->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        details->setPlainText(report.details);

        auto *close = new QPushButton("Close", &dialog);
        close->setFixedWidth(110);
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        buttonLayout->addWidget(close);

        layout->addWidget(summary);
        layout->addWidget(details, 1);
        layout->addLayout(buttonLayout);
        dialog.exec();
    });

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
