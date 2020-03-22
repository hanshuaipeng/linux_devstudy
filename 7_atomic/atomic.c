#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
 #include <linux/device.h>
 #include <linux/of.h>
 #include <linux/of_address.h>
 #include <linux/of_gpio.h>
 #include <asm/mach/map.h>
 #include <asm/uaccess.h>
 #include <asm/io.h>


#define DTS_NAME	"gpioled"
#define DTS_CNT		1
#define LED_ON		1
#define LED_OFF		0

struct gpioled_dev{
	dev_t devid;  /* 设备号 */
	struct cdev cdev;  /* cdev */
	struct class *class;  /* 类 */
	struct device *device;  /* 设备 */
	int major;  /* 主设备号 */
	int minor;  /* 次设备号 */
	struct device_node *nd; /* 设备节点 */
	int led_gpio[3];
	atomic_t lock; 		/* 原子变量 */
};

struct gpioled_dev gpioled;

static ssize_t gpioled_read (struct file *file, char __user *user, size_t cnt, loff_t *off_t)
{
    return 0;
}
static ssize_t gpioled_write (struct file *file, const char __user *user, size_t cnt, loff_t *off_t)
{
	int ret = 0;
	unsigned char databuf[2];

	struct gpioled_dev *dev = file->private_data;

	ret = copy_from_user(databuf, user, cnt);
	if(ret < 0)
	{
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}
	switch(databuf[0])
	{
		case 1:
			gpio_set_value(dev->led_gpio[0],databuf[1]);
		break;
		case 2:
			gpio_set_value(dev->led_gpio[1],databuf[1]);
		break;
		case 3:
			gpio_set_value(dev->led_gpio[2],databuf[1]);
		break;
		default :break;
	}
	return 0;
}

static int gpioled_open (struct inode *inode, struct file *file)
{
	/* 通过判断原子变量的值来检查 LED 有没有被别的应用使用 */
	if(!atomic_dec_and_test(&gpioled.lock))/*从 v 减 1，如果结果为 0 就返回真，否则返回假*/
	{
		atomic_inc(&gpioled.lock);	/* 小于 0 的话就加 1,使其原子变量等于 0 */
		return -EBUSY; 			/* LED 被使用，返回忙 */
	}
	file->private_data = &gpioled; /* 设置私有数据 */
    return 0;
}
static int gpioled_release (struct inode *inode, struct file *file)
{
	struct gpioled_dev *dev = file->private_data;
	/* 关闭驱动文件的时候释放原子变量 */
	atomic_inc(&dev->lock);
    return 0;
}
static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = gpioled_open,
    .write = gpioled_write,
    .read = gpioled_read,
    .release = gpioled_release,
};  

static int __init gpioled_init(void)
{
	int ret = 0;
	u8 i = 0;

	/* 初始化原子变量 */
	atomic_set(&gpioled.lock, 1); /* 原子变量初始值为 1 */
	/* 获取设备树中的属性数据 */
	 /* 1、获取设备节点： led */
	gpioled.nd = of_find_node_by_path("/gpio_led");
	if(gpioled.nd == NULL)
	{
		printk("can't find gpio_led\r\n");
		return -EINVAL;
	}
	else
	{
		printk("r_led node has been found! \r\n");
	}
	/* 2、 获取设备树中的 gpio 属性，得到 LED 所使用的 LED 编号 */
	for(i = 0; i<3; i++)
	{
		gpioled.led_gpio[i] = of_get_named_gpio(gpioled.nd, "led-gpios", i);
		printk("led_gpio[%d]=%d\r\n",i,gpioled.led_gpio[i]);
		if( gpioled.led_gpio[i] < 0)
		{
			printk("can't get led-gpio\r\n");
			return -EINVAL;
		}
	}
	/* 3、设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
	for(i = 0; i<3; i++)
	{
		ret = gpio_direction_output(gpioled.led_gpio[i], 1);
		if(ret < 0) {
			printk("can't set gpio %d! \r\n",gpioled.led_gpio[i]);
		}
	}

	if(gpioled.major)
	{
		gpioled.devid = MKDEV(gpioled.major,0);
		register_chrdev_region(gpioled.devid,DTS_CNT,DTS_NAME);
	}
	else
	{
		alloc_chrdev_region(&gpioled.devid, 0, DTS_CNT, DTS_NAME);/* 申请设备号 */
		gpioled.major = MAJOR(gpioled.devid);/* 获取分配号的主设备号 */
		gpioled.minor = MINOR(gpioled.devid);/* 获取分配号的次设备号 */
	}
	printk("gpioled major=%d,minor=%d\r\n", gpioled. major,gpioled. minor);
	/* 2、初始化 cdev */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);
	/* 3、添加一个 cdev */
	cdev_add(&gpioled.cdev,gpioled.devid,DTS_CNT);
	/* 4、创建类 */
	gpioled.class = class_create(THIS_MODULE,DTS_NAME);
	if(IS_ERR(gpioled.class))
    {
        return PTR_ERR(gpioled.class);
    }
	/* 5、创建设备 */
	gpioled.device = device_create(gpioled.class,NULL, gpioled.devid, NULL, DTS_NAME);
	if(IS_ERR(gpioled.device))
    {
        return PTR_ERR(gpioled.device);
    }
	
	printk("gpioled_init\r\n");
    return 0;
}

static void __exit gpioled_exit(void)
{

	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);
	unregister_chrdev_region(gpioled.devid, DTS_CNT); /*注销设备号*/

	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class);
	printk("gpioled_exit\r\n");
}
module_init(gpioled_init);
module_exit(gpioled_exit);
MODULE_LICENSE("GPL");
