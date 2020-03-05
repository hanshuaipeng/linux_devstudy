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

#define BEEP_ON     1
#define BEEP_OFF    0

/* beep -> GPIO1_19 */
#define BEEPDEV_NAME    "beep_dev"
#define BEEPDEV_CNT     1
/* 时钟寄存器 */
#define CCM_CCGR1_BASE                  (0X020C406C) //GPIO1时钟
/* IO模式选择寄存器 */
#define IOMUXC_SW_MUX_GPIO1_19_BASE     (0X020E0090)
#define IOMUXC_SW_PAD_GPIO1_19_BASE     (0x020E032C)
/* IO输入输出寄存器 */
#define GPIO1_DR_BASE                (0X0209C000)
#define GPIO1_GDIR_BASE              (0X0209C004)

static void __iomem *CCM_CCGR1;
static void __iomem *IOMUXC_SW_MUX_GPIO1_19;
static void __iomem *IOMUXC_SW_PAD_GPIO1_19;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct newchrled_dev{
    dev_t devid;        //设备ID
    struct cdev cdev;   
    struct class *beepclass;
    struct device *beepdevice;
    int major;
    int minor;
};
struct newchrled_dev beepdev;

static void beep_status(uint8_t onoff)
{
    u32 vul = 0;
    if(onoff == BEEP_ON)
    {
        vul = readl(GPIO1_DR);
        vul |= 1<<19;
        writel(vul,GPIO1_DR);
    }
    else if(onoff == BEEP_OFF)
    {
        vul = readl(GPIO1_DR);
        vul &= ~(1<<19);
        writel(vul,GPIO1_DR);
    }
    //printk("onoff=%d\r\n",onoff);
    //printk("vul=%x\r\n",vul);
}

static int beep_open (struct inode *inode, struct file *file)
{
    file->private_data = &beepdev;
    return 0;
}
static ssize_t beep_read (struct file *file, char __user *user, size_t cnt, loff_t *off_t)
{
    return 0;
}
static ssize_t beep_write (struct file *file, const char __user *user, size_t cnt, loff_t *off_t)
{
    int ret = 0;
    uint8_t wrietbuf;
    ret = copy_from_user(&wrietbuf, user, cnt);
    if(ret == 0)
    {
        beep_status(wrietbuf);
        //printk("wrietbuf=%d\r\n",wrietbuf);
    }
    return 0;
}
static int beep_release (struct inode *inode, struct file *file)
{
    return 0;
}


static const struct file_operations beep_ops = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .read = beep_read,
    .write = beep_write,
    .release = beep_release,
};

static int __init beep_init (void)
{
    u32 vul = 0;
    CCM_CCGR1               = ioremap(CCM_CCGR1_BASE,4);
    IOMUXC_SW_MUX_GPIO1_19  = ioremap(IOMUXC_SW_MUX_GPIO1_19_BASE, 4);  
    IOMUXC_SW_PAD_GPIO1_19  = ioremap(IOMUXC_SW_PAD_GPIO1_19_BASE, 4); 
    GPIO1_DR             = ioremap(GPIO1_DR_BASE, 4); 
    GPIO1_GDIR           = ioremap(GPIO1_GDIR_BASE, 4); 

    /* 使能时钟  */
    vul = readl(CCM_CCGR1);
    vul &= ~(3<<26);//
    vul |= 3<<26;//开启GPIO1时钟
    writel(vul,CCM_CCGR1);

    /* 设置IO复用功能  */
    writel(5,IOMUXC_SW_MUX_GPIO1_19);
    writel(0x10b0,IOMUXC_SW_PAD_GPIO1_19);

    /* 设置输出 */
    vul = readl(GPIO1_GDIR);
    vul |= 1<<19;
    writel(vul,GPIO1_GDIR);
    /* 初始化关闭蜂鸣器 */
    vul = readl(GPIO1_DR);
    vul &= ~(1<<19);
    writel(vul,GPIO1_DR);
    
    if(beepdev.major)
    {
        beepdev.devid = MKDEV(beepdev.major,0);
        register_chrdev_region(beepdev.devid,BEEPDEV_CNT,BEEPDEV_NAME);
    }
    else
    {
        alloc_chrdev_region(&beepdev.devid,0,BEEPDEV_CNT,BEEPDEV_NAME);
        beepdev.major = MAJOR(beepdev.devid);
        beepdev.minor = MINOR(beepdev.devid);
    }

    beepdev.cdev.owner = THIS_MODULE;
    cdev_init(&beepdev.cdev,&beep_ops);
    cdev_add(&beepdev.cdev,beepdev.devid,BEEPDEV_CNT);

    beepdev.beepclass = class_create(THIS_MODULE, BEEPDEV_NAME);
	if (IS_ERR(beepdev.beepclass)) 
    {
		return PTR_ERR(beepdev.beepclass); 
    }
    beepdev.beepdevice = device_create(beepdev.beepclass, NULL, beepdev.devid, NULL, BEEPDEV_NAME);
    if (IS_ERR(beepdev.beepdevice)) 
    {
		return PTR_ERR(beepdev.beepdevice); 
    }
    printk("beepdev init!!!\r\n");
    return 0;
}
static void __exit beep_exit (void)
{
    iounmap(CCM_CCGR1);
        
    iounmap(IOMUXC_SW_MUX_GPIO1_19);
    iounmap(IOMUXC_SW_PAD_GPIO1_19);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    
    cdev_del(&beepdev.cdev);
    unregister_chrdev_region(beepdev.devid,BEEPDEV_CNT);
    device_destroy(beepdev.beepclass,beepdev.devid);
    class_destroy(beepdev.beepclass);
    printk("beepdev exit!!!\r\n");
}

module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");


