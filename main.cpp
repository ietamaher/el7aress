#include "ui/mainwindow.h"

#include <QApplication>
#include "core/systemcontroller.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    SystemController sysCtrl;
    sysCtrl.initializeSystem();
    sysCtrl.showMainWindow();

    return app.exec();
}
