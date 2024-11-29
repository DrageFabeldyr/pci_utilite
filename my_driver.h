#ifndef MY_DRIVER_H
#define MY_DRIVER_H

#include <QObject>
#include <QTimer>
#include <sys/types.h>  // для ssize_t;

class MyDriver : public QObject
{
    Q_OBJECT

public:
    MyDriver();
    ~MyDriver();

private:
    QTimer *my_timer;
    int num_of_modules = 1;
    int fds[12];            // дескрипторы файлов устройств
    ssize_t read_res = 0;   // ssize_t используется, когда функция может вернуть или значение или отрицательную ошибку
    ssize_t write_res = 0;  // ssize_t используется, когда функция может вернуть или значение или отрицательную ошибку
    unsigned int buf = 0;   // сюда пишутся данные из модуля
    unsigned int bar0baseaddr, bar1baseaddr;
    int shift = 0;

private slots:
    void do_reg(void);
};

#endif // MY_DRIVER_H
