#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("MalachiteVR GameFix");
    QApplication::setApplicationVersion("0.3.0");
    QApplication::setOrganizationName("MalachiteeVR");

    MainWindow window;
    window.show();

    return app.exec();
}
