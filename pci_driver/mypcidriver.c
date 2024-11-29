#include <linux/module.h>	// обязательно для всех модулей
#include <linux/init.h>		// для init и exit
#include <linux/pci.h>
#include <linux/device.h>	// для device_create
#include <linux/cdev.h>		// для cdev_init, cdev_add
#include <linux/fs.h>		// для file_operations
#include <linux/random.h>	// для get_random_bytes

#include "mycommandlist.h"


// сетевая карта
//#define MY_VENDOR_ID 0x10EC
//#define MY_DEVICE_ID 0x8139

// PCI - COM конвертор
#define MY_VENDOR_ID 0x9710
#define MY_DEVICE_ID 0x9835

#define MAX_DEV 1			// пока непонятно - это количество модулей или количество каких-то сущностей внутри каждого модуля

/* test */
#define commandreg 0x0050 // command reg 93C46 offset
#define writeledenable 0xC0 // нужно записать в конфиг 0, чтобы можно было управлять конфиг 1
#define confreg1 0x0052 // config reg 1 offset
#define led1 0x80
#define led0 0x40
#define leds 0xC0


// метаданные
MODULE_LICENSE("GPL");
MODULE_AUTHOR("DF");
MODULE_DESCRIPTION("MY PCI DRIVER");


// таблица пар Vendor ID и Device ID, с которыми может работать драйвер:
static struct pci_device_id my_ids[] = {
	{ PCI_DEVICE(MY_VENDOR_ID, MY_DEVICE_ID) },
	{ } // обязательно завершать список с помощью пустого идентификатора
};


// макрос для передачи списка устройств, с которыми работает наш драйвер, в таблицу устройств ядра
// при подключении устройства ядро проверяет device table и ищет драйвер, который работает с vendor/deviceid, которые пришли от устройства
// если находит - загружает этот драйвер и инициализирует устройство
MODULE_DEVICE_TABLE(pci, my_ids);


// прототипы функций (функции должны быть определены выше точки использования)
static int 	my_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void my_remove(struct pci_dev *dev);
int create_char_dev(void);
int destroy_char_dev(void);
static int my_open(struct inode *inode, struct file *file);
static int my_release(struct inode *inode, struct file *file);
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);


// структура драйвера
static struct pci_driver my_driver = {
	.name = "my_driver",	// имя драйвера, которое будет использовано ядром в /sys/bus/pci/drivers
	.id_table = my_ids, 	// таблица пар Vendor ID и Device ID, с которыми может работать драйвер:
	.probe = my_probe,		// функция, вызываемая ядром после загрузки драйвера, служит для инициализации оборудования
	.remove = my_remove		// функция, вызываемая ядром при выгрузке драйвера, служит для освобождения каких-либо ранее занятых ресурсов
};


// структура файловых операций
static const struct file_operations my_fops = {
	.owner		= THIS_MODULE,
	.open		= my_open,
	.release	= my_release,
	.unlocked_ioctl = my_ioctl,
	.read		= my_read,
	.write		= my_write
};


// структура для связи файла устройства с символьным устройством
struct my_device_data
{
	struct device *my_device;	// файл устройства
	struct cdev my_cdev; 		// символьное устройство
};


void __iomem *ptr_bar0, *ptr_bar1;
static int dev_major = 0; // уникальный номер, выдаваемый драйверу
static int dev_minor = 0; // номер устройства, использующего драйвер
static unsigned long bar0baseaddr = 0;
static unsigned long bar1baseaddr = 0;
static struct class *my_class;	// класс для создания файла устройства
static struct my_device_data my_data[MAX_DEV]; // мои модули (не знаю, как это правильно обозвать)

/**
 * @brief 	функция входа в ядро Linux
 */
static int __init my_init(void)
{
	printk(KERN_INFO "my driver - registering the PCI device\n");
	return pci_register_driver(&my_driver);
}


/**
 * @brief 	функция выхода из ядра Linux
 */
static void __exit my_exit(void)
{
	printk(KERN_INFO "my driver - unregistering the PCI device\n");
	pci_unregister_driver(&my_driver);
}


/**
 * @brief 		функция чтения конфигурационного пространства PCI (вынесем отдельно, чтобы не нагромождать probe)
 *
 * @param dev	указатель на pci устройство
 *
 * @return 		0, если всё хорошо
 *				отрицательный код ошибки, если нет
 */
int read_device_config(struct pci_dev *dev)
{
	u16 vendor, device, status_reg, command_reg;

 	printk(KERN_INFO "my driver - read_device_config function\n"); // использование макросов KERN_INFO, KERN_ERR не обязательно

 	// читаем vendor
 	if (0 != pci_read_config_word(dev, PCI_VENDOR_ID, &vendor))
 	{
	 	printk(KERN_ERR "my driver - error reading vendor from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	// читаем deviceid
 	if (0 != pci_read_config_word(dev, PCI_DEVICE_ID, &device))
 	{
	 	printk(KERN_ERR "my driver - error reading deviceid from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	// выводим считанные vendor и deviceid
 	printk(KERN_INFO "my driver - our PCI device vid: 0x%X   pid: 0x%X\n", vendor, device);
 	
 	//читаем регистр статуса
 	if (0 != pci_read_config_word(dev, PCI_STATUS, &status_reg))
 	{
	 	printk(KERN_ERR "my driver - error reading status reg from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	printk(KERN_INFO "my driver - our PCI device status reg: 0x%X\n", status_reg);
 	
 	// читаем регистр команд
 	if (0 != pci_read_config_word(dev, PCI_COMMAND, &command_reg))
 	{
	 	printk(KERN_ERR "my driver - error reading command reg from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	printk(KERN_INFO "my driver - our PCI device command reg: 0x%X\n", command_reg);
 	
 	if (command_reg | PCI_COMMAND_MEMORY) // если выставлен бит доступа к памяти
 	{
 		printk(KERN_INFO "my driver - our PCI device supports memory access");
 		return 0;
 	}
	printk(KERN_ERR "my driver - our PCI device doesn't support memory access");
	return -EIO;
}


/**
 * @brief 		функция, вызываемая ядром после загрузки драйвера
 * 				служит для инициализации подключённого оборудования, а также для подключаемых в процессе устройств (hot plug)
 * 				если они есть в таблице устройств (pci_device_id)
 *
 * @param dev	указатель на pci устройство
 * @param id	указатель на конкретный элемент (пару значений) в таблице устройств
 *
 * @return 		0, если всё хорошо
 *				отрицательный код ошибки, если нет
 */
static int my_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int status, len; // для вывода всяких проверяемых значений
	//void __iomem *ptr_bar0, *ptr_bar1;
	
	printk(KERN_INFO "my driver - probe function\n");
	
	// читаем конфигурационные регистры
	if (read_device_config(dev) < 0)
	{
		printk(KERN_ERR "my driver - reading configuration registers failed\n");
		return -EIO;
	}
	
	// прочитаем размер BAR0
	len = pci_resource_len(dev, 0);
	printk(KERN_INFO "my driver - BAR0 is %d bytes in size\n", len);
	// прочитаем размер BAR1
	len = pci_resource_len(dev, 1);
	printk(KERN_INFO "my driver - BAR1 is %d bytes in size\n", len);
	// найдём их начальные адреса
	bar0baseaddr = pci_resource_start(dev, 0);
	printk(KERN_INFO "my driver - BAR0 is mapped to 0x%lx\n", bar0baseaddr);
	bar1baseaddr = pci_resource_start(dev, 1);
	printk(KERN_INFO "my driver - BAR1 is mapped to 0x%lx\n", bar1baseaddr);
	
	status = pcim_enable_device(dev); // если использовать pci_ вместо pcim_, придётся делать pci_disable_device, а так система сама всё сделает
	if (status < 0)
	{
		printk(KERN_ERR "my driver - could not enable device; status = %d\n", status);
		return status;
	}
	
	status = pcim_iomap_regions(dev, BIT(0), KBUILD_MODNAME); // устройство, битовая маска, имя области (строка), тут вместо строки макрос какой-то
	if (status < 0)
	{
		printk(KERN_ERR "my driver - BAR0 is already in use; status = %d\n", status);
		return status;
	}
	status = pcim_iomap_regions(dev, BIT(1), KBUILD_MODNAME); // устройство, битовая маска, имя области (строка), тут вместо строки макрос какой-то
	if (status < 0)
	{
		printk(KERN_ERR "my driver - BAR1 is already in use; status = %d\n", status);
		return status;
	}
	
	ptr_bar0 = pcim_iomap_table(dev)[0];
	if (ptr_bar0 == NULL)
	{
		printk(KERN_ERR "my driver - BAR0 pointer is invalid\n");
		return -1;
	}
	ptr_bar1 = pcim_iomap_table(dev)[1];
	if (ptr_bar1 == NULL)
	{
		printk(KERN_ERR "my driver - BAR1 pointer is invalid\n");
		return -1;
	}
	
	/* test:
	printk(KERN_INFO "my driver - command reg 93C46 is 0x%x\n", ioread8(ptr_bar0 + commandreg));
	printk(KERN_INFO "my driver - command reg 93C46 write 0x%x\n", writeledenable);
	iowrite8(writeledenable, ptr_bar0 + commandreg);
	printk(KERN_INFO "my driver - command reg 93C46 is 0x%x\n", ioread8(ptr_bar0 + commandreg));
	printk(KERN_INFO "my driver - config register 1 is 0x%x\n", ioread8(ptr_bar0 + confreg1));
	printk(KERN_INFO "my driver - config register 1 write 0x%x\n", leds);
	iowrite8(leds, ptr_bar0 + confreg1);
	printk(KERN_INFO "my driver - config register 1 is 0x%x\n", ioread8(ptr_bar0 + confreg1));
	*/
	
	// создаём символьное устройство для обмена данными между драйвером и GUI
	create_char_dev();
	
	return 0;
}


/**
 * @brief 		функция, вызываемая ядром при выгрузке драйвера, служит для освобождения каких-зибо ранее занятых ресурсов
 *
 * @param dev	указатель на pci устройство
 */
static void my_remove(struct pci_dev *dev)
{
	printk(KERN_INFO "my driver - remove function\n");
	
	// удаляем символьное устройство
	destroy_char_dev();
}


/**
 * @brief 		функция, создающая символьное устройство для обмена данными между драйвером и GUI
 *
 * @return 			0, если всё хорошо
 *					-1 или код ошибки, если нет
 */
int create_char_dev(void)
{
	int status; // для вывода всяких проверяемых значений
	dev_t dev;	// идентификатор нашего устройства, сюда запишутся его major и minor
	int i; // тут нельзя определить его внутри for
	
	printk(KERN_INFO "my driver - create_char_dev function\n");

	status = alloc_chrdev_region(&dev, 0, MAX_DEV, "my_dev");
	if (status < 0)
	{
		printk(KERN_ERR "my driver - could not allocate major number for device; status = %d\n", status);
		return status;
	}
	dev_major = MAJOR(dev); // "номер" драйвера
	dev_minor = MINOR(dev); // "номер" устройства
	printk(KERN_INFO "my driver - major = %d, minor = %d\n", dev_major, dev_minor);

	// создадим класс
	my_class = class_create(THIS_MODULE, "my_class");
	if (IS_ERR(my_class))
	{
		printk(KERN_ERR "my driver - could not create class structure for device\n");
		goto err_class;
	}
	else
		printk(KERN_INFO "my driver - class structure for device created\n");
	
	// создадим устройства
	status = 0;
	for (i = 0; i < MAX_DEV; i++)
	{
		// инициализируем символьное устройство
		cdev_init(&my_data[i].my_cdev, &my_fops);
		// добавим символьное устройство в систему
		cdev_add(&my_data[i].my_cdev, MKDEV(dev_major, i), 1); // последний параметр - количество символьных устройств, ассоциируемых  с устройством; обычно равен 1
		
		// добавим устройство в систему, в системе появятся /dev/my_device-0, /dev/my_device-1 и т.д.
		my_data[i].my_device = device_create(my_class, NULL, MKDEV(dev_major, i), NULL, "my_device-%d", i);
		if (IS_ERR(my_data[i].my_device))
		{
			printk(KERN_ERR "my driver - could not create device%d\n", i);
			status++;
		}
		else
			printk(KERN_INFO "my driver - device /dev/my_device-%d created\n", i);
	}
	if (status == MAX_DEV) // ни одно устройство не создалось
		goto err_device;

	return 0;
	
	
err_device:
	class_destroy(my_class);
err_class:
	unregister_chrdev_region(dev, MAX_DEV); // тут можно просто dev
	
	return -1;
}


/**
 * @brief 		функция, удаляющая символьное устройство
 *
 * @return 		0, если всё хорошо
 */
int destroy_char_dev(void)
{
	int i; // тут нельзя определить его внутри for

	printk(KERN_INFO "my driver - destroy_char_dev function\n");

	for (i = 0; i < MAX_DEV; i++)
		device_destroy(my_class, MKDEV(dev_major, i));
	class_destroy(my_class);
	unregister_chrdev_region(MKDEV(dev_major, 0), MAX_DEV); // найдём dev с помощью MKDEV
	
	return 0;
}


/**
 * @brief 			функция, открывающая файл устройства
 *
 * @param inode		файл
 * @param file		дескриптор файла
 *
 * @return 			0, если всё хорошо
 */
static int my_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "my driver - open function\n");
	
	// установим разрешение на запись регистров конфигурации
	printk(KERN_INFO "my driver - command reg 93C46 is 0x%x\n", ioread8(ptr_bar0 + commandreg));
	printk(KERN_INFO "my driver - command reg 93C46 write 0x%x\n", writeledenable);
	iowrite8(writeledenable, ptr_bar0 + commandreg);
	printk(KERN_INFO "my driver - command reg 93C46 is 0x%x\n", ioread8(ptr_bar0 + commandreg));

	return 0;
}


/**
 * @brief 			функция, закрывающая файл устройства
 *
 * @param inode		файл
 * @param file		дескриптор файла
 *
 * @return 			0, если всё хорошо
 */
static int my_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "my driver - release function\n");
	
	return 0;
}


/**
 * @brief 			функция передачи команд в устройство
 *
 * @param file		дескриптор файла
 * @param cmd		номер команды
 * @param arg		
 *
 * @return 			
 */
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
		case GET_BAR0_ADDR:
			return bar0baseaddr;
			
		case GET_BAR1_ADDR:
			return bar1baseaddr;
		
		default:
			return -EINVAL;
	};
	
	return 0;
}


/**
 * @brief 			функция чтения данных с устройства
 *
 * @param file		дескриптор файла
 * @param buf
 * @param count
 * @param offset
 *
 * @return 			количество считанных байт
 */
static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	int res, status;
	//printk(KERN_INFO "my driver - read function\n");
	
	//printk(KERN_INFO "my driver - config register 1 is 0x%x\n", ioread8(ptr_bar0 + confreg1));
	res = ioread8(ptr_bar0 + *offset);
	status = copy_to_user(buf, &res, sizeof(res)); // addr to in user space, addr from in kernel space, num of bytes to copy
	if (status) // 0 - всё хорошо, иначе количество байт, которые не удалось скопировать
	{
		printk(KERN_ERR "my driver - error copying to user, can't copy %d bytes\n", status);
		return -EFAULT;
	}
	printk(KERN_INFO "my driver - config register 1 is 0x%x\n", res);

	return sizeof(res);
}


/**
 * @brief 			функция записи данных в устройство
 *
 * @param file		дескриптор файла
 * @param buf
 * @param count
 * @param offset
 *
 * @return 			количество записанных байт
 */
static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	//int i;
	int res, status;
	
	//printk(KERN_INFO "my driver - write function\n");
	
	//get_random_bytes(&i, sizeof(i)); // генерим случайное число
	//res = (i % 4) * 0x40;
	status = copy_from_user(&res, buf, count); // addr to in kernel space, addr from in user space, num of bytes to copy
	if (status) // 0 - всё хорошо, иначе количество байт, которые не удалось скопировать
	{
		printk(KERN_ERR "my driver - error copying from user, can't copy %d bytes\n", status);
		return -EFAULT;
	}
	printk(KERN_INFO "my driver - config register 1 write 0x%x\n", res);
	//iowrite8(res, ptr_bar0 + confreg1);
	iowrite8(res, ptr_bar0 + *offset);

	return count;
}


// эти макросы нужны для какой-то там ассоциации
module_init(my_init);
module_exit(my_exit);

