#include <QCoreApplication>

#include "my_driver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    MyDriver *mDrv = new MyDriver();

    return a.exec();
}
