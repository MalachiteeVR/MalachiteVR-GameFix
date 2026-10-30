#include "MainWindow.h"
#include "SystemInfo.h"

#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("MalachiteVR GameFix");
    resize(900, 620);
    setMinimumSize(760, 520);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(18);

    auto *title = new QLabel("MalachiteVR GameFix", central);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *subtitle = new QLabel("Find out why your games are running poorly.", central);
    subtitle->setObjectName("subtitle");

    auto *grid = new QGridLayout;
    grid->setSpacing(14);

    auto makeCard = [central, grid](int row, int column, const QString &heading, QLabel *&value) {
        auto *box = new QGroupBox(heading, central);
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(16, 14, 16, 16);
        value = new QLabel("Detecting...", box);
        value->setWordWrap(true);
        QFont font = value->font();
        font.setPointSize(13);
        font.setBold(true);
        value->setFont(font);
        layout->addWidget(value);
        grid->addWidget(box, row, column);
    };

    makeCard(0, 0, "CPU", cpuValue_);
    makeCard(0, 1, "GPU", gpuValue_);
    makeCard(1, 0, "Memory", memoryValue_);
    makeCard(1, 1, "Operating System", osValue_);
    makeCard(2, 0, "Architecture", architectureValue_);
    makeCard(2, 1, "GameFix Status", statusValue_);

    auto *buttonRow = new QHBoxLayout;
    auto *refreshButton = new QPushButton("Refresh System Information", central);
    auto *diagnosticButton = new QPushButton("Run Full Diagnostic", central);
    diagnosticButton->setEnabled(false);
    buttonRow->addWidget(refreshButton);
    buttonRow->addWidget(diagnosticButton);

    auto *footer = new QLabel("v0.1.0  •  Windows  •  Qt 6  •  C++20", central);
    footer->setObjectName("footer");

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addLayout(grid);
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
    refreshSystemInfo();
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
