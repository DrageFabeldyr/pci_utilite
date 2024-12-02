/* список команд, используемых для вызова ioctl
 * просто копируем этот файл в папку с программой и в папку с драйвером,
 * чтобы ничего не забыть и всё совпадало
 * */

#ifndef MYCOMMANDLIST_H
#define MYCOMMANDLIST_H

#define GET_BAR0_ADDR 0x50
#define GET_BAR1_ADDR 0x51

#define SET_BAR0 0x60
#define SET_BAR1 0x61

#endif

