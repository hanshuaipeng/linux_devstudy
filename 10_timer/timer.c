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

#define DTS_NAME	"timerledS"
#define DTS_CNT		1
#define LED_ON		1
#define LED_OFF		0

/*
_IO    an ioctl with no parameters
_IO(type,nr) //没有参数的命令
_IOR(type,nr,size) //该命令是从驱动读取数据

_IOW(type,nr,size) //该命令是从驱动写入数据

_IOWR(type,nr,size) //双向数据传输

幻数(type)、序号(nr)和大小(size)
幻数参考linux-imx-rel_imx_4.1.15_2.1.0_ga/Documentation/ioctl/ioctl-number.txt
幻数：说得再好听的名字也只不过是个0~0xff的数，占8bit(_IOC_TYPEBITS)。
	这个数是用来区分不同的驱动的，像设备号申请的时候一样，内核有一个文档给出一些推荐的或者已经被使用的幻数。
填写未使用的幻数即可
*/

#define CLOSE_CMD			(_IO(0xef,0x01))	/* 关闭定时器 */
#define OPEN_CMD			(_IO(0xef,0x02))
#define SETPERIOD_CMD		(_IO(0xef,0x03))	/* 设置定时器周期命令 */


struct timer_dev{
	dev_t devid;  /* 设备号 */
	struct cdev cdev;  /* cdev */
	struct class *class;  /* 类 */
	struct device *device;  /* 设备 */
	int major;  /* 主设备号 */
	int minor;  /* 次设备号 */
	struct device_node *nd; /* 设备节点 */
	int led_gpio[3];
	struct timer_list timer; /* 定义一个定时器 */
	spinlock_t lock; 	/* 定义自旋锁 */
	int timerperiod; 		/* 定时周期,单位为ms */
};

struct timer_dev timerdev;

/*
* @description : 初始化 LED 灯 IO， open 函数打开驱动的时候
* 初始化 LED 灯所使用的 GPIO 引脚。
* @param : 无
* @return : 无
*/
static int led_init(void)
{
	int ret = 0;
	u8 i = 0;
	/* 获取设备树中的属性数据 */
	 /* 1、获取设备节点： led */
	timerdev.nd = of_find_node_by_path("/gpio_led");
	if(timerdev.nd == NULL)
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
		timerdev.led_gpio[i] = of_get_named_gpio(timerdev.nd, "led-gpios", i);
		printk("led_gpio[%d]=%d\r\n",i,timerdev.led_gpio[i]);
		if( timerdev.led_gpio[i] < 0)
		{
			printk("can't get led-gpio\r\n");
			return -EINVAL;
		}
	}
	/* 3、设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
	for(i = 0; i<3; i++)
	{
		ret = gpio_direction_output(timerdev.led_gpio[i], 1);
		if(ret < 0) {
			printk("can't set gpio %d! \r\n",timerdev.led_gpio[i]);
		}
	}
	return 0;
}

static ssize_t timer_read (struct file *file, char __user *user, size_t cnt, loff_t *off_t)
{
    return 0;
}
static ssize_t timer_write (struct file *file, const char __user *user, size_t cnt, loff_t *off_t)
{
	int ret = 0;
	unsigned char databuf[2];

	struct timer_dev *dev = file->private_data;

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

long timer_unlocked_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct timer_dev *dev = filp->private_data;
	int timerperiod;
	unsigned long flags;

	switch(cmd)
	{
		case CLOSE_CMD:
			del_timer_sync(&dev->timer);
			break;
		case OPEN_CMD:
			spin_lock_irqsave(&timerdev.lock,flags);
			timerperiod = dev->timerperiod;
			spin_unlock_irqrestore(&timerdev.lock,flags);
			mod_timer(&timerdev.timer,jiffies+msecs_to_jiffies(timerperiod));
			break;
		case SETPERIOD_CMD:
			spin_lock_irqsave(&timerdev.lock,flags);/*自旋锁*/
			dev->timerperiod = arg;
			spin_unlock_irqrestore(&timerdev.lock,flags);
			mod_timer(&timerdev.timer,jiffies+msecs_to_jiffies(arg));/*将毫秒、微秒、纳秒转换为 jiffies 类型。*/
			break;
	}
}
/* 定时器回调 */
void timer_function(unsigned long arg)
{
	struct timer_dev *dev = (struct timer_dev *)arg;
	static int sta = 1;
	int timerperiod;
	unsigned long flags;

	sta = !sta;		/* 每次都取反，实现LED灯反转 */
	gpio_set_value(dev->led_gpio[0], sta);
	
	/* 重启定时器 */
	spin_lock_irqsave(&dev->lock, flags);
	timerperiod = dev->timerperiod;
	spin_unlock_irqrestore(&dev->lock, flags);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timerperiod)); 
}

static int timer_open (struct inode *inode, struct file *filp)
{
	int ret = 0;
	filp->private_data = &timerdev; /* 设置私有数据 */
	ret = led_init();
	if (ret < 0) 
	{
		return ret;
	}
	return 0;

}
static int timer_release (struct inode *inode, struct file *filp)
{
	struct timer_dev *dev = filp->private_data;
	
    return 0;
}
static struct file_operations timerdev_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .write = timer_write,
    .read = timer_read,
    .release = timer_release,
	.unlocked_ioctl = timer_unlocked_ioctl,
};  

static int __init timer_init(void)
{
	if(timerdev.major)
	{
		timerdev.devid = MKDEV(timerdev.major,0);
		register_chrdev_region(timerdev.devid,DTS_CNT,DTS_NAME);
	}
	else
	{
		alloc_chrdev_region(&timerdev.devid, 0, DTS_CNT, DTS_NAME);/* 申请设备号 */
		timerdev.major = MAJOR(timerdev.devid);/* 获取分配号的主设备号 */
		timerdev.minor = MINOR(timerdev.devid);/* 获取分配号的次设备号 */
	}
	printk("timer major=%d,minor=%d\r\n", timerdev. major,timerdev. minor);
	/* 2、初始化 cdev */
	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timerdev_fops);
	/* 3、添加一个 cdev */
	cdev_add(&timerdev.cdev,timerdev.devid,DTS_CNT);
	/* 4、创建类 */
	timerdev.class = class_create(THIS_MODULE,DTS_NAME);
	if(IS_ERR(timerdev.class))
    {
        return PTR_ERR(timerdev.class);
    }
	/* 5、创建设备 */
	timerdev.device = device_create(timerdev.class,NULL, timerdev.devid, NULL, DTS_NAME);
	if(IS_ERR(timerdev.device))
    {
        return PTR_ERR(timerdev.device);
    }
	/* 初始化定时器 */
	init_timer(&timerdev.timer);
	timerdev.timer.function = timer_function;
	timerdev.timer.data = (unsigned long)&timerdev;

	printk("timer_init\r\n");
    return 0;
}

static void __exit timer_exit(void)
{
	gpio_set_value(timerdev.led_gpio[0], 1);	/* 卸载驱动的时候关闭LED */
	gpio_set_value(timerdev.led_gpio[1], 1);	/* 卸载驱动的时候关闭LED */
	gpio_set_value(timerdev.led_gpio[2], 1);	/* 卸载驱动的时候关闭LED */
	del_timer_sync(&timerdev.timer);		/* 删除timer */
#if 0
	del_timer(&timerdev.tiemr);
#endif

	/* 注销字符设备驱动 */
	cdev_del(&timerdev.cdev);
	unregister_chrdev_region(timerdev.devid, DTS_CNT); /*注销设备号*/

	device_destroy(timerdev.class, timerdev.devid);
	class_destroy(timerdev.class);
	printk("timer_exit\r\n");
}
module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
