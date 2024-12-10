#include <linux/module.h>	// обязательно для всех модулей
#include <linux/init.h>		// для init и exit
#include <linux/pci.h>		// 
#include <linux/device.h>	// для device_create			// для работы с файлами устройств
#include <linux/cdev.h>		// для cdev_init, cdev_add		// для работы с символьными устройствами
#include <linux/fs.h>		// для file_operations
#include <linux/random.h>	// для get_random_bytes
#include <linux/list.h>		// 
#include <linux/mutex.h>	// 

#include "mycommandlist.h"	// список команд для ioctl


// PCI - COM конвертор
#define MY_VENDOR_ID 0x9710
#define MY_DEVICE_ID 0x9835


// метаданные
MODULE_LICENSE("GPL");
MODULE_AUTHOR("DF");
MODULE_DESCRIPTION("MY PCI DRIVER");


// глобальные переменные
LIST_HEAD(card_list);			// список PCI-устройств
static struct mutex lock;
static int card_count = 0;		// количество PCI-устройств
static int dev_major = 0; 		// уникальный номер, выдаваемый драйверу
static struct class *my_class;	// класс для создания файла устройства


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
static int 	my_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void my_remove(struct pci_dev *pdev);
static int my_open(struct inode *inode, struct file *file);
static int my_release(struct inode *inode, struct file *file);
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);


// структура драйвера
static struct pci_driver my_driver = {
	.name = "mypcidriver",	// имя драйвера, которое будет использовано ядром в /sys/bus/pci/drivers
	.id_table = my_ids, 	// таблица пар Vendor ID и Device ID, с которыми может работать драйвер:
	.probe = my_probe,		// функция, вызываемая ядром после загрузки драйвера, служит для инициализации оборудования
	.remove = my_remove		// функция, вызываемая ядром при выгрузке драйвера, служит для освобождения каких-либо ранее занятых ресурсов
};


// структура файловых операций
static const struct file_operations my_fops = {
	.owner		= THIS_MODULE,		// всегда, если не оговорено иное
	.open		= my_open,
	.release	= my_release,
	.unlocked_ioctl = my_ioctl,
	.read		= my_read,
	.write		= my_write
};


// структура PCI-устройства
struct my_device {
	struct pci_dev *pdev;						// указатель на само устройство
	void __iomem *ptr_bar0, *ptr_bar1, *addr;	// указатели на bar0, bar1 и вспомогательный для переключения bar'ов при чтении
	struct list_head list;						// позиция в списке
	struct cdev cdev;							// связанное символьное устройство
	dev_t dev_nr;								// номер устройства
	unsigned long bar0addr, bar1addr;			// начальные адреса bar0, bar1
	struct device *device_file;					// файл устройства
};


// функция входа в ядро Linux
static int __init my_init(void)
{
	int status; 	// для вывода всяких проверяемых значений
	dev_t dev_nr;	// номер нашего устройства (major, minor)
	
	printk(KERN_INFO "my driver - __init, registering driver\n");

	status = alloc_chrdev_region(&dev_nr, 0, MINORMASK + 1, "my_dev"); // переменная для номера, начальный минор, количество устройств (сейчас = допустимому максимуму), имя
	if (status < 0)
	{
		printk(KERN_ERR "my driver - could not allocate major number for device; status = %d\n", status);
		return status;
	}
	dev_major = MAJOR(dev_nr); // "номер" драйвера
	printk(KERN_INFO "my driver - major (driver number) = %d\n", dev_major);
	
	// создадим класс, в системе появится /sys/class/my_class
	my_class = class_create(THIS_MODULE, "my_class");
	if (IS_ERR(my_class))
	{
		printk(KERN_ERR "my driver - could not create class structure for device\n");
		unregister_chrdev_region(dev_nr, MINORMASK + 1);
		return -1;
	}
	else
		printk(KERN_INFO "my driver - class structure for device created\n");

	// инициализируем мьютекс
	mutex_init(&lock);

	// зарегистрируем драйвер в системе
	status = pci_register_driver(&my_driver);
	if (status < 0)
	{
		printk(KERN_ERR "my driver - error registering the driver\n");
		class_destroy(my_class);
		unregister_chrdev_region(dev_nr, MINORMASK + 1);
		return status;
	}
	
	return 0;
}


// функция выхода из ядра Linux
static void __exit my_exit(void)
{
	printk(KERN_INFO "my driver - __exit, unregistering driver\n");

	class_destroy(my_class);
	unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK + 1);
	pci_unregister_driver(&my_driver);
}


// функция чтения конфигурационного пространства PCI (вынесем отдельно, чтобы не нагромождать probe)
int read_device_config(struct pci_dev *pdev)
{
	u16 vendor, device, status_reg, command_reg;

 	printk(KERN_INFO "my driver - read_device_config function\n"); // использование макросов KERN_INFO, KERN_ERR не обязательно

 	// читаем vendor
 	if (0 != pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor))
 	{
	 	printk(KERN_ERR "my driver - error reading vendor from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	// читаем deviceid
 	if (0 != pci_read_config_word(pdev, PCI_DEVICE_ID, &device))
 	{
	 	printk(KERN_ERR "my driver - error reading deviceid from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	// выводим считанные vendor и deviceid
 	printk(KERN_INFO "my driver - our PCI device vid: 0x%X   pid: 0x%X\n", vendor, device);
 	
 	//читаем регистр статуса
 	if (0 != pci_read_config_word(pdev, PCI_STATUS, &status_reg))
 	{
	 	printk(KERN_ERR "my driver - error reading status reg from config space\n");
	 	return -EIO; // отрицательный код ошибки из pci_read_config_word
 	}
 	printk(KERN_INFO "my driver - our PCI device status reg: 0x%X\n", status_reg);
 	
 	// читаем регистр команд
 	if (0 != pci_read_config_word(pdev, PCI_COMMAND, &command_reg))
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


// функция, вызываемая ядром после загрузки драйвера
// служит для инициализации подключённого оборудования, а также для подключаемых в процессе устройств (hot plug)
// если они есть в таблице устройств (pci_device_id)
static int my_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int status, len;	// для вывода всяких проверяемых значений
	int dev_minor = 0;	// номер устройства, использующего драйвер

	struct my_device *my;
	
	printk(KERN_INFO "my driver - probe function\n");
	
	my = devm_kzalloc(&pdev->dev, sizeof(struct my_device), GFP_KERNEL); // выделим память(кому, сколько, GFP флаг)
	if (!my)
		return -ENOMEM; 
	
	mutex_lock(&lock);
	
	// инициализируем символьное устройство
	cdev_init(&my->cdev, &my_fops);
	// выдадим ему номер
	my->dev_nr = MKDEV(dev_major, card_count++);
	// добавим символьное устройство в систему (теперь ядро сможет вызывать его функции (file operations))
	status = cdev_add(&my->cdev, my->dev_nr, 1); // последний параметр - количество символьных устройств, ассоциируемых с PCI-устройством; обычно равен 1
	if (status < 0)
	{
		printk(KERN_ERR "my driver - error adding cdev\n");
		return status;
	}
	// добавим его в наш список устройств
	list_add_tail(&my->list, &card_list);
	
	// добавим устройство в систему, в системе появятся /dev/my_device-0 (теперь мы сможем обращаться к нему из пространства пользователя)
	my->device_file = device_create(my_class, NULL, my->dev_nr, NULL, "my_device-%d", MINOR(my->dev_nr)); // класс, родительское устр-во, dev_t, доп.данные, имя
	if (IS_ERR(my->device_file))
	{
		printk(KERN_ERR "my driver - could not create device%d\n", 0);
		return -1;
	}
	else
		printk(KERN_INFO "my driver - device /dev/my_device-%d created\n", MINOR(my->dev_nr));

	
	mutex_unlock(&lock);
	
	my->pdev = pdev;
	
	// читаем конфигурационные регистры
	status = read_device_config(my->pdev);
	if (status < 0)
	{
		printk(KERN_ERR "my driver - reading configuration registers failed\n");
		return status;
	}
	
	dev_minor = MINOR(my->dev_nr); // "номер" устройства
	printk(KERN_INFO "my driver - device number = %d, major = %d, minor = %d\n", my->dev_nr, dev_major, dev_minor);
	// прочитаем размер BAR0
	len = pci_resource_len(my->pdev, 0);
	printk(KERN_INFO "my driver - BAR0 is %d bytes in size\n", len);
	// прочитаем размер BAR1
	len = pci_resource_len(my->pdev, 1);
	printk(KERN_INFO "my driver - BAR1 is %d bytes in size\n", len);
	// найдём их начальные адреса
	my->bar0addr = pci_resource_start(my->pdev, 0);
	printk(KERN_INFO "my driver - BAR0 is mapped to 0x%lx\n", my->bar0addr);
	my->bar1addr = pci_resource_start(my->pdev, 1);
	printk(KERN_INFO "my driver - BAR1 is mapped to 0x%lx\n", my->bar1addr);
	
	status = pcim_enable_device(my->pdev); // если использовать pci_ вместо pcim_, придётся делать pci_disable_device, а так система сама всё сделает
	if (status < 0)
	{
		printk(KERN_ERR "my driver - could not enable device; status = %d\n", status);
		return status;
	}
	
	status = pcim_iomap_regions(my->pdev, BIT(0), KBUILD_MODNAME); // устройство, битовая маска, имя области (строка), тут вместо строки макрос какой-то
	if (status < 0)
	{
		printk(KERN_ERR "my driver - BAR0 is already in use; status = %d\n", status);
		return status;
	}
	status = pcim_iomap_regions(my->pdev, BIT(1), KBUILD_MODNAME); // устройство, битовая маска, имя области (строка), тут вместо строки макрос какой-то
	if (status < 0)
	{
		printk(KERN_ERR "my driver - BAR1 is already in use; status = %d\n", status);
		return status;
	}
	
	my->ptr_bar0 = pcim_iomap_table(my->pdev)[0];
	if (my->ptr_bar0 == NULL)
	{
		printk(KERN_ERR "my driver - BAR0 pointer is invalid\n");
		list_del(&my->list);
		cdev_del(&my->cdev);
		return -1;
	}
	my->ptr_bar1 = pcim_iomap_table(my->pdev)[1];
	if (my->ptr_bar1 == NULL)
	{
		printk(KERN_ERR "my driver - BAR1 pointer is invalid\n");
		list_del(&my->list);
		cdev_del(&my->cdev);
		return -1;
	}
	
	return 0;
}


// функция, вызываемая ядром при выгрузке драйвера, служит для освобождения каких-зибо ранее занятых ресурсов
static void my_remove(struct pci_dev *pdev)
{
	struct my_device *my, *next;
	
	printk(KERN_INFO "my driver - remove device function\n");
	list_for_each_entry_safe(my, next, &card_list, list) // safe нужен, потому что мы редактируем список
	{
		if (my->pdev == pdev)
		{
			list_del(&my->list);
			cdev_del(&my->cdev);
		}
	}
}


// функция, открывающая файл устройства
static int my_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "my driver - open function\n");
	
	struct my_device *my;
	dev_t dev_nr = inode->i_rdev;
	
	list_for_each_entry(my, &card_list, list) // safe не нужен, потому что мы не трогаем сам список
	{
		if (my->dev_nr == dev_nr) // нашли в списке вызвавшее функцию устройство
		{
			file->private_data = my;
			return 0;
		}
	}
	return -ENODEV; // error no device
}


// функция, закрывающая файл устройства
static int my_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "my driver - release function\n");
	
	return 0;
}


// функция передачи команд в устройство
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct my_device *my = (struct my_device *)file->private_data;

	switch (cmd)
	{
		case GET_BAR0_ADDR:
			return my->bar0addr;
			
		case GET_BAR1_ADDR:
			return my->bar1addr;
		
		case SET_BAR0:
			my->addr = my->ptr_bar0;
			break;
		
		case SET_BAR1:
			my->addr = my->ptr_bar1;
			break;
		
		default:
			return -EINVAL;
	};
	
	return 0;
}


// функция чтения данных с устройства
static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	int res, status;
	struct my_device *my = (struct my_device *)file->private_data;
	
	res = ioread8(my->addr + *offset);
	status = copy_to_user(buf, &res, sizeof(res)); // addr to in user space, addr from in kernel space, num of bytes to copy
	if (status) // 0 - всё хорошо, иначе количество байт, которые не удалось скопировать
	{
		printk(KERN_ERR "my driver - error copying to user, can't copy %d bytes\n", status);
		return -EFAULT;
	}
	printk(KERN_INFO "my driver - register 0x%x read: 0x%x\n", my->addr, res);

	return sizeof(res);
}


// функция записи данных в устройство
static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	int res, status;
	struct my_device *my = (struct my_device *)file->private_data;
	
	status = copy_from_user(&res, buf, count); // addr to in kernel space, addr from in user space, num of bytes to copy
	if (status) // 0 - всё хорошо, иначе количество байт, которые не удалось скопировать
	{
		printk(KERN_ERR "my driver - error copying from user, can't copy %d bytes\n", status);
		return -EFAULT;
	}
	printk(KERN_INFO "my driver - register 0x%x write: 0x%x\n", my->addr, res);
	iowrite8(res, my->addr + *offset);

	return count;
}


// эти макросы нужны для какой-то там ассоциации
module_init(my_init);
module_exit(my_exit);

