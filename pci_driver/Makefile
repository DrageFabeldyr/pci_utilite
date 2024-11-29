obj-m += mypcidriver.o

#при вводе make
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /lib/modules/`uname -r`/build M=$(PWD) modules
	
#при вводе make clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
#	make -C /lib/modules/`uname -r`/build M=$(PWD) clean
	
# obj-m 	- переменная утилиты make; сборочные скрипты ядра, вызываемые перечисленными командами, берут из неё имена модулей (имена файлов с исходными кодами), из которых будет собран модуль ядра.
# make -C 	- это указание утилите make текущего каталога, где искать Makefile, соответственно build - это конечное место сборки.
# M 		- переменная окружения, указывает сборочным скриптам ядра, гденаходятся исходники и куда поместить результат сборки.
# PWD		- текущая директория
# $(var) 	- переменная var
# $(shell uname -r) - версия ядра
