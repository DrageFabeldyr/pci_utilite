# объявление переменных
BINARY		:= mypcidriver
TARGET_PATH := /lib/modules/$(shell uname -r)/kernel/drivers/my

obj-m += $(BINARY).o


#при вводе make
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

	
#при вводе make clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
#	make -C /lib/modules/`uname -r`/build M=$(PWD) clean


#при вводе make install
install:
	mkdir $(TARGET_PATH)
	cp $(BINARY).ko $(TARGET_PATH)
	depmod -a


	
# obj-m 	- переменная утилиты make; сборочные скрипты ядра, вызываемые перечисленными командами, берут из неё имена модулей (имена файлов с исходными кодами), из которых будет собран модуль ядра.
# make -C 	- это указание утилите make текущего каталога, где искать Makefile, соответственно build - это конечное место сборки.
# M 		- переменная окружения, указывает сборочным скриптам ядра, где находятся исходники и куда поместить результат сборки.
# PWD		- текущая директория
# $(var) 	- переменная var
# $(shell uname -r) - версия ядра
