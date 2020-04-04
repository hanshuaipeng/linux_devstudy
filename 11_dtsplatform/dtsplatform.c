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
#include <linux/timer.h>
#include <linux/platform_device.h>


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
	file->private_data = &gpioled; /* 设置私有数据 */
    return 0;
}
static int gpioled_release (struct inode *inode, struct file *file)
{
    return 0;
}
static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = gpioled_open,
    .write = gpioled_write,
    .read = gpioled_read,
    .release = gpioled_release,
};  
/*
struct of_device_id {
	char	name[32];
	char	type[32];
	char	compatible[128];
	const void *data;
};
*/
static const struct of_device_id led_of_match[] = {
	{ .compatible =  "rgb-led"},//属性值，跟设备树中的属性一直
	{	}
};

int led_probe(struct platform_device *dev)
{
	int ret = 0;
	u8 i = 0;
	printk("led driver and device was matched!\r\n");
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
	printk("gpioled_init\r\n");
	return 0;
}
int led_remove(struct platform_device *dev)
{
	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);
	unregister_chrdev_region(gpioled.devid, DTS_CNT); /*注销设备号*/

	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class);
	printk("gpioled_exit\r\n");
	return 0;
}

/* platform 驱动结构体 */
static struct platform_driver led_driver = {
	.driver = {
		.name = "imx6ul-led", /* 驱动名字，用于和设备匹配 */
		.of_match_table = led_of_match, /* 设备树匹配表 */
	},
	.probe = led_probe,
	.remove = led_remove,
};

static int __init gpioled_init(void)
{
	return platform_driver_register(&led_driver);
}

static void __exit gpioled_exit(void)
{
	platform_driver_unregister(&led_driver);
}
module_init(gpioled_init);
module_exit(gpioled_exit);
MODULE_LICENSE("GPL");
