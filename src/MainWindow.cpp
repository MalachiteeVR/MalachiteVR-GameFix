#include "MainWindow.h"

#include "Diagnostic.h"
#include "Fixes.h"
#include "PerformanceHistory.h"
#include "SystemInfo.h"
#include "Telemetry.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("MalachiteVR GameFix");
    resize(1060, 860);
    setMinimumSize(720, 560);

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
    systemGrid->addWidget(makeCard("Detected Game", gameValue_), 2, 1);
    systemGrid->addWidget(makeCard("GameFix Status", statusValue_), 3, 0, 1, 2);

    auto *telemetryTitle = new QLabel("Live Performance", central);
    QFont telemetryFont = telemetryTitle->font();
    telemetryFont.setPointSize(16);
    telemetryFont.setBold(true);
    telemetryTitle->setFont(telemetryFont);

    auto *telemetrySubtitle = new QLabel(
        "Updated every second. Values are read-only and stay on this PC. Per-core utilization helps catch lightly-threaded CPU bottlenecks.", central);
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
    telemetryGrid->addWidget(makeCard("Per-Core CPU Usage", perCoreValue_), 2, 1);
    telemetryGrid->addWidget(makeCard("Bottleneck Analysis", diagnosisValue_), 3, 0, 1, 2);

    auto *captureBox = new QGroupBox("Performance Capture", central);
    auto *captureLayout = new QHBoxLayout(captureBox);
    captureLayout->setContentsMargins(16, 14, 16, 16);
    captureStatusValue_ = new QLabel("Not capturing. Start a capture while a recognized game is running.", captureBox);
    captureStatusValue_->setWordWrap(true);
    captureButton_ = new QPushButton("Start Capture", captureBox);
    captureButton_->setMinimumWidth(140);
    captureLayout->addWidget(captureStatusValue_, 1);
    captureLayout->addWidget(captureButton_);

    auto *buttonGrid = new QGridLayout;
    buttonGrid->setSpacing(10);
    auto *refreshButton = new QPushButton("Refresh System Information", central);
    auto *diagnosticButton = new QPushButton("Run Full Diagnostic", central);
    auto *historyButton = new QPushButton("Performance History", central);
    auto *fixesButton = new QPushButton("Safe Gaming Fixes", central);
    refreshButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    diagnosticButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    historyButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fixesButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttonGrid->addWidget(refreshButton, 0, 0);
    buttonGrid->addWidget(diagnosticButton, 0, 1);
    buttonGrid->addWidget(historyButton, 1, 0);
    buttonGrid->addWidget(fixesButton, 1, 1);

    auto *footer = new QLabel("v0.4.0  •  Windows  •  Qt 6  •  C++20  •  Diagnostics + capture + reversible fixes", central);
    footer->setObjectName("footer");
    footer->setWordWrap(true);

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addLayout(systemGrid);
    root->addWidget(telemetryTitle);
    root->addWidget(telemetrySubtitle);
    root->addLayout(telemetryGrid);
    root->addWidget(captureBox);
    root->addStretch(1);
    root->addLayout(buttonGrid);
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
        QPlainTextEdit, QComboBox {
            background: #080c0a;
            border: 1px solid #26352c;
            border-radius: 8px;
            padding: 8px;
            color: #e8f5ec;
            selection-background-color: #205a34;
        }
        QComboBox QAbstractItemView {
            background: #101712;
            color: #e8f5ec;
            selection-background-color: #205a34;
        }
    )");

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshSystemInfo);

    connect(captureButton_, &QPushButton::clicked, this, [this]() {
        if (captureActive_) {
            captureActive_ = false;
            captureGameName_.clear();
            captureButton_->setText("Start Capture");
            captureStatusValue_->setText("Capture stopped. Saved samples remain available in Performance History.");
            return;
        }

        if (!detectedGame_.running) {
            QMessageBox::information(this, "No Game Detected",
                                     "Start a recognized game first, then begin capture. GameFix will identify the game's executable and record performance once per second.");
            return;
        }

        captureActive_ = true;
        captureGameName_ = detectedGame_.name;
        captureButton_->setText("Stop Capture");
        captureStatusValue_->setText(QString("Capturing %1. Samples are stored locally until the history limit is reached.").arg(captureGameName_));
    });

    connect(historyButton, &QPushButton::clicked, this, &MainWindow::showHistoryDialog);
    connect(fixesButton, &QPushButton::clicked, this, &MainWindow::showFixesDialog);

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
        dialog.resize(900, 720);
        dialog.setMinimumSize(660, 500);

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

        auto *save = new QPushButton("Save Report...", &dialog);
        auto *close = new QPushButton("Close", &dialog);
        close->setFixedWidth(110);
        save->setFixedWidth(130);
        connect(save, &QPushButton::clicked, &dialog, [&dialog, report]() {
            const QString path = QFileDialog::getSaveFileName(
                &dialog,
                "Save GameFix Diagnostic Report",
                QString("GameFix-%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss")),
                "Text files (*.txt);;All files (*.*)");
            if (path.isEmpty())
                return;
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(&dialog, "Save Failed", "GameFix could not write the selected report file.");
                return;
            }
            QTextStream stream(&file);
            stream << report.details;
        });
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        buttonLayout->addWidget(save);
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
    detectedGame_ = detectRunningGame();

    if (detectedGame_.running) {
        gameValue_->setText(QString("%1\n%2")
                                .arg(detectedGame_.name)
                                .arg(detectedGame_.processName));
    } else {
        gameValue_->setText("No recognized game running");
    }

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
    const double ramPercent = telemetry.totalMemoryBytes > 0
                                  ? static_cast<double>(usedRam) / telemetry.totalMemoryBytes * 100.0
                                  : 0.0;
    ramUsageValue_->setText(QString("%1 / %2 (%3%)")
                                .arg(formatBytes(usedRam))
                                .arg(formatBytes(telemetry.totalMemoryBytes))
                                .arg(QString::number(ramPercent, 'f', 0)));

    if (telemetry.gpuMemoryTotalBytes > 0) {
        const double vramPercent = static_cast<double>(telemetry.gpuMemoryUsedBytes) /
                                   telemetry.gpuMemoryTotalBytes * 100.0;
        vramUsageValue_->setText(QString("%1 / %2 (%3%)")
                                     .arg(formatBytes(telemetry.gpuMemoryUsedBytes))
                                     .arg(formatBytes(telemetry.gpuMemoryTotalBytes))
                                     .arg(QString::number(vramPercent, 'f', 0)));
    } else {
        vramUsageValue_->setText("Unavailable");
    }

    if (telemetry.gpuTemperatureAvailable) {
        gpuTemperatureValue_->setText(QString("%1 °C")
                                          .arg(QString::number(telemetry.gpuTemperatureCelsius, 'f', 0)));
    } else {
        gpuTemperatureValue_->setText("Unavailable");
    }

    if (telemetry.perCoreUsageAvailable) {
        double hottest = 0.0;
        int hottestIndex = -1;
        for (int i = 0; i < telemetry.perCoreUsagePercent.size(); ++i) {
            if (telemetry.perCoreUsagePercent.at(i) > hottest) {
                hottest = telemetry.perCoreUsagePercent.at(i);
                hottestIndex = i;
            }
        }
        perCoreValue_->setText(QString("%1 cores sampled\nHottest: Core %2 at %3%")
                                   .arg(telemetry.perCoreUsagePercent.size())
                                   .arg(hottestIndex + 1)
                                   .arg(QString::number(hottest, 'f', 0)));
    } else {
        perCoreValue_->setText("Sampling...");
    }

    diagnosisValue_->setText(diagnoseBottleneck(telemetry));

    if (captureActive_) {
        if (!detectedGame_.running || detectedGame_.name != captureGameName_) {
            captureActive_ = false;
            captureGameName_.clear();
            captureButton_->setText("Start Capture");
            captureStatusValue_->setText("Capture stopped because the detected game changed or closed.");
        } else if (!PerformanceHistory::appendSample(captureGameName_, telemetry)) {
            captureStatusValue_->setText("Capture is running, but GameFix could not write the history file.");
        }
    }
}

void MainWindow::showHistoryDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("MalachiteVR GameFix - Performance History");
    dialog.resize(900, 680);
    dialog.setMinimumSize(650, 480);

    auto *layout = new QVBoxLayout(&dialog);
    auto *combo = new QComboBox(&dialog);
    const QStringList gameNames = PerformanceHistory::games();
    combo->addItems(gameNames);

    auto *summary = new QLabel(&dialog);
    summary->setWordWrap(true);
    auto *details = new QPlainTextEdit(&dialog);
    details->setReadOnly(true);
    details->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto refreshHistory = [combo, summary, details]() {
        const QString game = combo->currentText();
        const QVector<HistorySample> samples = PerformanceHistory::loadSamples(game, 600);
        summary->setText(PerformanceHistory::summarize(samples));

        QStringList lines;
        lines << "Timestamp | CPU | GPU | RAM | VRAM | GPU Temp";
        lines << "--------------------------------------------------------------";
        for (const HistorySample &sample : samples) {
            lines << QString("%1 | %2% | %3% | %4% | %5% | %6")
                          .arg(sample.timestamp)
                          .arg(QString::number(sample.cpuUsagePercent, 'f', 0))
                          .arg(QString::number(sample.gpuUsagePercent, 'f', 0))
                          .arg(QString::number(sample.ramUsagePercent, 'f', 0))
                          .arg(QString::number(sample.vramUsagePercent, 'f', 0))
                          .arg(sample.gpuTemperatureAvailable
                                   ? QString::number(sample.gpuTemperatureCelsius, 'f', 0) + " °C"
                                   : "n/a");
        }
        details->setPlainText(lines.join("\n"));
    };

    auto *buttonLayout = new QHBoxLayout;
    auto *exportButton = new QPushButton("Export CSV...", &dialog);
    auto *clearButton = new QPushButton("Clear Selected", &dialog);
    auto *closeButton = new QPushButton("Close", &dialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(closeButton);

    if (gameNames.isEmpty()) {
        summary->setText("No performance history exists yet. Start a capture while a recognized game is running.");
        exportButton->setEnabled(false);
        clearButton->setEnabled(false);
    } else {
        refreshHistory();
    }

    connect(combo, &QComboBox::currentTextChanged, &dialog, [refreshHistory]() { refreshHistory(); });
    connect(exportButton, &QPushButton::clicked, &dialog, [&dialog, combo]() {
        const QString game = combo->currentText();
        const QVector<HistorySample> samples = PerformanceHistory::loadSamples(game, 7200);
        if (samples.isEmpty())
            return;

        QString fileStem = game;
        fileStem.replace(' ', '_');
        const QString path = QFileDialog::getSaveFileName(
            &dialog,
            "Export GameFix Performance History",
            QString("%1-GameFix-History.csv").arg(fileStem),
            "CSV files (*.csv);;All files (*.*)");
        if (path.isEmpty())
            return;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(&dialog, "Export Failed", "GameFix could not write the selected CSV file.");
            return;
        }
        QTextStream stream(&file);
        stream << "timestamp,cpu_percent,gpu_percent,ram_percent,vram_percent,gpu_temperature_c\n";
        for (const HistorySample &sample : samples) {
            stream << sample.timestamp << ','
                   << sample.cpuUsagePercent << ','
                   << sample.gpuUsagePercent << ','
                   << sample.ramUsagePercent << ','
                   << sample.vramUsagePercent << ','
                   << (sample.gpuTemperatureAvailable ? QString::number(sample.gpuTemperatureCelsius) : QString())
                   << '\n';
        }
    });
    connect(clearButton, &QPushButton::clicked, &dialog, [combo, refreshHistory]() {
        if (combo->currentText().isEmpty())
            return;
        if (QMessageBox::question(nullptr, "Clear History",
                                  QString("Delete all saved performance samples for %1?").arg(combo->currentText())) != QMessageBox::Yes)
            return;
        const QString game = combo->currentText();
        if (PerformanceHistory::clearGame(game)) {
            combo->removeItem(combo->currentIndex());
            if (combo->count() > 0)
                refreshHistory();
        }
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    layout->addWidget(new QLabel("Select a captured game:", &dialog));
    layout->addWidget(combo);
    layout->addWidget(summary);
    layout->addWidget(details, 1);
    layout->addLayout(buttonLayout);
    dialog.exec();
}

void MainWindow::showFixesDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("MalachiteVR GameFix - Safe Gaming Fixes");
    dialog.resize(700, 600);
    dialog.setMinimumSize(580, 500);

    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(
        "These changes are opt-in. GameFix saves the previous value where possible and provides a restore action. Nothing here is applied automatically.",
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto addFix = [&dialog, layout](const QString &title, const QString &description,
                                    const std::function<FixResult()> &action) {
        auto *box = new QGroupBox(title, &dialog);
        auto *boxLayout = new QVBoxLayout(box);
        auto *descriptionLabel = new QLabel(description, box);
        descriptionLabel->setWordWrap(true);
        auto *button = new QPushButton("Apply", box);
        boxLayout->addWidget(descriptionLabel);
        boxLayout->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, &dialog, [box, title, action]() {
            if (QMessageBox::question(box, title,
                                      "Apply this change? GameFix will not make the change until you confirm this dialog.") != QMessageBox::Yes)
                return;
            const FixResult result = action();
            QMessageBox::information(box, result.title, result.message);
        });
        layout->addWidget(box);
    };

    addFix("Enable Windows Game Mode",
            "Enables the current user's Windows Game Mode setting. This is a per-user setting and can be restored.",
            [] { return FixManager::enableGameMode(); });

    addFix("Use High Performance Power Plan",
            "Activates Windows' High Performance power plan and remembers the plan that was active before the change.",
            [] { return FixManager::useHighPerformancePowerPlan(); });

    auto *fullscreenBox = new QGroupBox("Disable Fullscreen Optimizations for Detected Game", &dialog);
    auto *fullscreenLayout = new QVBoxLayout(fullscreenBox);
    auto *fullscreenLabel = new QLabel(
        detectedGame_.running
            ? QString("Detected: %1\n%2").arg(detectedGame_.name, detectedGame_.executablePath)
            : "No recognized game executable is currently available.",
        fullscreenBox);
    fullscreenLabel->setWordWrap(true);
    auto *fullscreenButton = new QPushButton("Apply", fullscreenBox);
    fullscreenButton->setEnabled(detectedGame_.running && !detectedGame_.executablePath.isEmpty());
    fullscreenLayout->addWidget(fullscreenLabel);
    fullscreenLayout->addWidget(fullscreenButton);
    connect(fullscreenButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        if (QMessageBox::question(&dialog, "Disable Fullscreen Optimizations",
                                  "Apply the per-game compatibility flag to the detected executable?") != QMessageBox::Yes)
            return;
        const FixResult result = FixManager::disableFullscreenOptimizations(detectedGame_.executablePath);
        QMessageBox::information(&dialog, result.title, result.message);
    });
    layout->addWidget(fullscreenBox);

    addFix("Enable Hardware-Accelerated GPU Scheduling",
            "Writes the Windows GraphicsDrivers HAGS setting. This requires running GameFix as administrator and restarting Windows.",
            [] { return FixManager::enableHardwareAcceleratedGpuScheduling(); });

    addFix("Disable Hardware-Accelerated GPU Scheduling",
            "Turns off the Windows GraphicsDrivers HAGS setting. This requires running GameFix as administrator and restarting Windows.",
            [] { return FixManager::disableHardwareAcceleratedGpuScheduling(); });

    auto *restoreBox = new QGroupBox("Restore GameFix Changes", &dialog);
    auto *restoreLayout = new QVBoxLayout(restoreBox);
    auto *restoreLabel = new QLabel("Restores saved Game Mode, power plan, fullscreen compatibility, and HAGS changes where backups exist.", restoreBox);
    restoreLabel->setWordWrap(true);
    auto *restoreButton = new QPushButton("Restore Saved Changes", restoreBox);
    restoreLayout->addWidget(restoreLabel);
    restoreLayout->addWidget(restoreButton);
    connect(restoreButton, &QPushButton::clicked, &dialog, [&dialog]() {
        if (QMessageBox::question(&dialog, "Restore Saved Changes",
                                  "Restore all GameFix-managed settings that have saved backups?") != QMessageBox::Yes)
            return;

        const FixResult gameMode = FixManager::restoreGameMode();
        const FixResult power = FixManager::restorePowerPlan();
        const FixResult fullscreen = FixManager::restoreFullscreenOptimizations();
        const FixResult hags = FixManager::restoreHardwareAcceleratedGpuScheduling();
        const QString message = QString("Game Mode: %1\nPower plan: %2\nFullscreen flags: %3\nHAGS: %4")
                                    .arg(gameMode.message)
                                    .arg(power.message)
                                    .arg(fullscreen.message)
                                    .arg(hags.message);
        QMessageBox::information(&dialog, "Restore Results", message);
    });
    layout->addWidget(restoreBox);

    auto *close = new QPushButton("Close", &dialog);
    close->setFixedWidth(110);
    auto *closeRow = new QHBoxLayout;
    closeRow->addStretch();
    closeRow->addWidget(close);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addLayout(closeRow);

    dialog.exec();
}
