#include "my_driver.h"
#include "pci_driver/mycommandlist.h"

#include <QDebug>
#include <fcntl.h>      // для open()
#include <unistd.h>     // для read()
#include <sys/ioctl.h>  // для ioctl()

MyDriver::MyDriver()
{
    QString inHex = "";

    qDebug() << "Hello!";
    QTimer *my_timer = new QTimer();
    connect(my_timer, &QTimer::timeout, this, &MyDriver::do_reg);

    for (int i = 0; i < num_of_devices; i++)
    {
        // путь до файла модуля
        QString str = QString("/dev/my_device-%1").arg(i);
        QByteArray ba = str.toLocal8Bit();
        const char *devpath = ba.data();

        qDebug() << "Opening " << devpath << " ...";
        fds[i] = open(devpath, O_RDWR);
        if (fds[i] == -1)
            qDebug() << "Opening failed! Error: " << strerror(errno);
        else
        {
            qDebug() << "Ok!";
            baraddr = ioctl(fds[i], GET_BAR0_ADDR, 0);
            inHex = "0x" + QString("%1").arg(baraddr, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "my_device-" << i << " bar0 base addr is: " << inHex << "(" << baraddr << ")";
            baraddr = ioctl(fds[i], GET_BAR1_ADDR, 0);
            if (baraddr > 0xFFFFFFFF) // почему-то однажды так случилось при выводе
            {
                inHex = "0x" + QString("%1").arg(baraddr , 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
                qDebug() << "Warning!! Wrong my_device-" << i << " bar1 address format:" << inHex << "(" << baraddr << ")";
                baraddr &= 0xFFFFFFFF;
            }
            inHex = "0x" + QString("%1").arg(baraddr , 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "my_device-" << i << " bar1 base addr is: " << inHex << "(" << baraddr << ")";
        }
    }


    my_timer->start(1000);
}

MyDriver::~MyDriver()
{
    for (int i = 0; i < num_of_devices; i++)
        if (fds[i] != -1) // а нужна ли эта проверка?
            close(fds[i]);
}


// реализация чтения/записи регистров по таймеру
void MyDriver::do_reg()
{
    QString inHex = "";

    for (int i = 0; i < num_of_devices; i++)
    {
        if (fds[i] != -1)
        {
            /* для PCI-COM конвертора */
            qDebug() << "my_device-" << i << "-------------------->";
            // читаем
            ioctl(fds[i], SET_BAR0, 0);
            res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            if (res == -1)
                qDebug() << "Error while reading bar0: " << strerror(errno);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar0 byte" << shift << ": " << inHex;
            ioctl(fds[i], SET_BAR1, 0);
            res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            if (res == -1)
                qDebug() << "Error while reading bar1: " << strerror(errno);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar1 byte" << shift << ": " << inHex;

            // пишем
            unsigned int data0 = (qrand() % 4) * 0x55; // случайным образом заполним
            inHex = "0x" + QString("%1").arg(data0, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data to write: " << inHex;
            buf = data0;
            ioctl(fds[i], SET_BAR0, 0);
            res = pwrite(fds[i], (void*)&buf, sizeof(buf), shift);
            if (res == -1)
                qDebug() << "Error while writing bar0: " << strerror(errno);
            unsigned int data1 = (qrand() % 4) * 0x55; // случайным образом заполним
            inHex = "0x" + QString("%1").arg(data1, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data to write: " << inHex;
            buf = data1;
            ioctl(fds[i], SET_BAR1, 0);
            res = pwrite(fds[i], (void*)&buf, sizeof(buf), shift);
            if (res == -1)
                qDebug() << "Error while writing bar1: " << strerror(errno);

            // читаем снова
            ioctl(fds[i], SET_BAR0, 0);
            res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            if (res == -1)
                qDebug() << "Error while reading bar0: " << strerror(errno);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar0 byte" << shift << ": " << inHex;
            ioctl(fds[i], SET_BAR1, 0);
            res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            if (res == -1)
                qDebug() << "Error while reading bar1: " << strerror(errno);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar1 byte" << shift << ": " << inHex;
            //*/
        }
    }
    /* для PCI-COM конвертора */
    shift++;
    if (shift >= 8)
    {
        shift = 0;
        qDebug() << "----------new-circle----------";
    }
    //*/
}
