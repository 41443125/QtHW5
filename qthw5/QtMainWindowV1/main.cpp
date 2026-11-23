#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Qt Image Studio"));

    MainWindow window;
    window.show();
    return app.exec();
}
