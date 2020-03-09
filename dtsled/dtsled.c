#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>

/*
GPIO1-04 
	r_led{
		#address-cells = <1>;
		#size-cells = <1>;
		compatible = "red-led";
		status = "okay";
		reg = <	0X020C406C 0x04         /* CCM_CCGR1_BASE 
				0X020E006C 0x04        /*  SW_MUX_GPIO1_IO04_BASE 
		      	0X020E02F8 0x04	       /*  SW_PAD_GPIO1_IO04_BASE 
               	0X0209C000 0x04         /* GPIO1_DR_BASE 
             	0X0209C004 0x04         /* GPIO1_GDIR_BASE 
		>
	};
*/
#define DTS_NAME	"dtsled"
#define DTS_CNT		1
#define LED_ON		1
#define LED_OFF		0


static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO04; 
static void __iomem *SW_PAD_GPIO1_IO04; 
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct dtsled_dev{
	dev_t devid;  /* 设备号 */
	struct cdev cdev;  /* cdev */
	struct class *class;  /* 类 */
	struct device *device;  /* 设备 */
	int major;  /* 主设备号 */
	int minor;  /* 次设备号 */
	struct device_node *nd; /* 设备节点 */
};

struct dtsled_dev dtsled;

void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LED_ON) {
		val = readl(GPIO1_DR);
		val &= ~(1 << 4); 
		writel(val, GPIO1_DR);
	} 
	else if(sta == LED_OFF) {
		val = readl(GPIO1_DR);
		val|= (1 << 4);
		writel(val, GPIO1_DR);
	} 
}


static ssize_t dtsled_read (struct file *file, char __user *user, size_t cnt, loff_t *off_t)
{
    return 0;
}
static ssize_t dtsled_write (struct file *file, const char __user *user, size_t cnt, loff_t *off_t)
{
	int ret = 0;
	unsigned char databuf[1];
	unsigned char ledstat;

	ret = copy_from_user(databuf, user, cnt);
	if(ret < 0)
	{
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}
	ledstat = databuf[0];
	printk("ledstat = %d\r\n",ledstat);
	led_switch(ledstat);
    return 0;
}

static int dtsled_open (struct inode *inode, struct file *file)
{
    return 0;
}
static int dtsled_release (struct inode *inode, struct file *file)
{
    return 0;
}
static struct file_operations dtsled_fops = {
    .owner = THIS_MODULE,
    .open = dtsled_open,
    .write = dtsled_write,
    .read = dtsled_read,
    .release = dtsled_release,
};  

static int __init dtsled_init(void)
{
	int ret = 0;
	u32 vul = 0;
	u32 regdata[14];
	const char *str;
	struct property *proper;
	/* 获取设备树中的属性数据 */
	 /* 1、获取设备节点： r_led */
	dtsled.nd = of_find_node_by_path("/r_led");
	if(dtsled.nd == NULL)
	{
		printk("can't find r_led\r\n");
		return -EINVAL;
	}
	else
	{
		printk("r_led node has been found! \r\n");
	}
	/* 2、获取 compatible 属性内容 */
	proper = of_find_property(dtsled.nd,"compatible",NULL);
	if(dtsled.nd == NULL)
	{
		printk("can't find compatible\r\n");
	}
	else
	{
		printk("compatible is %s \r\n",proper->value);
	}
	/* 3、获取 status 属性内容 */
	ret = of_property_read_string(dtsled.nd, "status", &str);
	if(ret < 0)
	{
		printk("status read failed! \r\n");
	} else 
	{
		printk("status = %s\r\n", str);
	}
	/* 4、获取 reg 属性内容 */
	ret = of_property_read_u32_array(dtsled.nd,"reg",regdata,10);
	if(ret < 0) 
	{
		printk("reg property read failed! \r\n");
	} 
	else 
	{
		u8 i = 0;
		printk("reg data: \r\n");
		for(i = 0; i < 10; i++)
			printk("%#X ", regdata[i]);
		printk("\r\n");
	}

	/* 1、寄存器地址映射 */
	CCM_CCGR1 = ioremap(regdata[0], regdata[1]);
	SW_MUX_GPIO1_IO04 = ioremap(regdata[2], regdata[3]);
	SW_PAD_GPIO1_IO04 = ioremap(regdata[4], regdata[5]);
	GPIO1_DR = ioremap(regdata[6], regdata[7]);
	GPIO1_GDIR = ioremap(regdata[8], regdata[9]);

	/* 使能时钟  */
    vul = readl(CCM_CCGR1);
    vul &= ~(3<<26);//
    vul |= 3<<26;//开启GPIO1时钟
    writel(vul,CCM_CCGR1);

/* 设置IO复用功能  */
    writel(5,SW_MUX_GPIO1_IO04);
    writel(0x10b0,SW_PAD_GPIO1_IO04);

/* 设置输出 */
    vul = readl(GPIO1_GDIR);
    vul |= 1<<4;
    writel(vul,GPIO1_GDIR);

	if(dtsled.major)
	{
		dtsled.devid = MKDEV(dtsled.major,0);
		register_chrdev_region(dtsled.devid,DTS_CNT,DTS_NAME);
	}
	else
	{
		alloc_chrdev_region(&dtsled.devid, 0, DTS_CNT, DTS_NAME);/* 申请设备号 */
		dtsled.major = MAJOR(dtsled.devid);/* 获取分配号的主设备号 */
		dtsled.minor = MINOR(dtsled.devid);/* 获取分配号的次设备号 */
	}
	printk("dtsled major=%d,minor=%d\r\n", dtsled. major,dtsled. minor);
	/* 2、初始化 cdev */
	dtsled.cdev.owner = THIS_MODULE;
	cdev_init(&dtsled.cdev, &dtsled_fops);
	/* 3、添加一个 cdev */
	cdev_add(&dtsled.cdev,dtsled.devid,DTS_CNT);
	/* 4、创建类 */
	dtsled.class = class_create(THIS_MODULE,DTS_NAME);
	if(IS_ERR(dtsled.class))
    {
        return PTR_ERR(dtsled.class);
    }
	/* 5、创建设备 */
	dtsled.device = device_create(dtsled.class,NULL, dtsled.devid, NULL, DTS_NAME);
	if(IS_ERR(dtsled.device))
    {
        return PTR_ERR(dtsled.device);
    }
	
	printk("dtsled_init\r\n");
    return 0;
}

static void __exit dtsled_exit(void)
{
	/* 取消映射 */
	iounmap(CCM_CCGR1);
        
    iounmap(SW_MUX_GPIO1_IO04);
    iounmap(SW_PAD_GPIO1_IO04);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

	/* 注销字符设备驱动 */
	cdev_del(&dtsled.cdev);
	unregister_chrdev_region(dtsled.devid, DTS_CNT); /*注销设备号*/

	device_destroy(dtsled.class, dtsled.devid);
	class_destroy(dtsled.class);
	printk("dtsled_exit\r\n");
}
module_init(dtsled_init);
module_exit(dtsled_exit);
MODULE_LICENSE("GPL");
