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

    for (int i = 0; i < num_of_modules; i++)
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
            bar0baseaddr = ioctl(fds[i], GET_BAR0_ADDR, 0);
            inHex = "0x" + QString("%1").arg(bar0baseaddr, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "my_device-" << i << " bar0 base addr is: " << inHex << "(" << bar0baseaddr << ")";
            bar1baseaddr = ioctl(fds[i], GET_BAR1_ADDR, 0);
            if (bar1baseaddr > 0xFFFFFFFF)
            {
                inHex = "0x" + QString("%1").arg(bar1baseaddr , 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
                qDebug() << "Warning!! Wrong my_device-" << i << " bar1 address format:" << inHex << "(" << bar1baseaddr << ")";
                bar1baseaddr &= 0xFFFFFFFF;
            }
            inHex = "0x" + QString("%1").arg(bar1baseaddr , 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "my_device-" << i << " bar1 base addr is: " << inHex << "(" << bar1baseaddr << ")";
        }
    }


    my_timer->start(1000);
}

MyDriver::~MyDriver()
{
    for (int i = 0; i < num_of_modules; i++)
        if (fds[i] != -1) // а нужна ли эта проверка?
            close(fds[i]);
}


// реализация чтения/записи регистров по таймеру
void MyDriver::do_reg()
{
    QString inHex = "", addrHex = "";
    off_t reg_addr = /*bar0baseaddr +*/ 0x0052; // адрес регистра внутри bar0


    for (int i = 0; i < num_of_modules; i++)
    {
        if (fds[i] != -1)
        {

            /* для сетевой
            qDebug() << "tick! ->";
            // читаем
            ioctl(fds[i], SET_BAR0, 0);
            read_res = pread(fds[i], (void*)&buf, sizeof(buf), reg_addr);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            addrHex = "0x" + QString("%1").arg(reg_addr, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from " << addrHex << " register: " << inHex;

            // пишем
            unsigned int data = (qrand() % 4) * 0x40; // случайным образом заполним два старших бита
            inHex = "0x" + QString("%1").arg(data, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data to write: " << inHex;
            buf = data;
            //write_res = write(fds[i], (void*)&buf, sizeof(buf));
            write_res = pwrite(fds[i], (void*)&buf, sizeof(buf), reg_addr);
            //*/


            /* для PCI-COM конвертора */
            qDebug() << "tack! ->";
            // читаем
            ioctl(fds[i], SET_BAR0, 0);
            read_res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar0 byte" << shift << ": " << inHex;
            ioctl(fds[i], SET_BAR1, 0);
            read_res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar1 byte" << shift << ": " << inHex;

            // пишем
            //unsigned int data = (qrand() % 4) * 0x40; // случайным образом заполним два старших бита
            unsigned int data0 = (qrand() % 4) * 0x55; // случайным образом заполним
            inHex = "0x" + QString("%1").arg(data0, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data to write: " << inHex;
            buf = data0;
            ioctl(fds[i], SET_BAR0, 0);
            write_res = pwrite(fds[i], (void*)&buf, sizeof(buf), shift);
            unsigned int data1 = (qrand() % 4) * 0x55; // случайным образом заполним
            inHex = "0x" + QString("%1").arg(data1, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data to write: " << inHex;
            buf = data1;
            ioctl(fds[i], SET_BAR1, 0);
            write_res = pwrite(fds[i], (void*)&buf, sizeof(buf), shift);

            // читаем снова
            ioctl(fds[i], SET_BAR0, 0);
            read_res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar0 byte" << shift << ": " << inHex;
            ioctl(fds[i], SET_BAR1, 0);
            read_res = pread(fds[i], (void*)&buf, sizeof(buf), shift);
            inHex = "0x" + QString("%1").arg(buf, 2, 16, QChar('0')).toUpper(); // чтобы было красиво, например "0x0C", а не "c"
            qDebug() << "data from bar1 byte" << shift << ": " << inHex;

            shift++;
            if (shift >= 8)
                shift = 0;
                //*/
        }
    }

}
