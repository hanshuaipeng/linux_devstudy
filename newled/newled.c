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

#define NEWLED_CNT      1
#define NEWLEN_NAME     "newled"

#define LEDOFF      0
#define LEDON       1

static char writebuf[10];

/*
    GPIO_4          ->LED_R     --GPIO1-04 
    CSI_VSYNC        ->LED_B     --GPIO4-19
    CSI_HSYNC       ->LED_G     --GPIO4-20
*/
/* 寄存器物理地址 */
#define CCM_CCGR1_BASE              (0X020C406C) //GPIO1时钟
#define CCM_CCGR3_BASE              (0X020C4074) //GPIO4时钟
/* GPIO_4          ->LED_R     --GPIO1-04 */
#define SW_MUX_GPIO1_IO04_BASE      (0X020E006C)
#define SW_PAD_GPIO1_IO04_BASE      (0X020E02F8)
#define GPIO1_DR_BASE               (0X0209C000)
#define GPIO1_GDIR_BASE             (0X0209C004)
/* CSI_VSYNC        ->LED_B     --GPIO4-19 */
#define SW_MUX_GPIO4_IO19_BASE      (0X020E01DC)
#define SW_PAD_GPIO4_IO19_BASE      (0X020E0468)
#define GPIO4_DR_BASE               (0X020A8000)
#define GPIO4_GDIR_BASE             (0X020A8004)
/* CSI_HSYNC       ->LED_G     --GPIO4-20 */
#define SW_MUX_GPIO4_IO20_BASE      (0X020E01E0)
#define SW_PAD_GPIO4_IO20_BASE      (0X020E046C)

/* 寄存器映射后虚拟地址 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *IMX6U_CCM_CCGR3;
/* GPIO_4          ->LED_R     --GPIO1-04 */
static void __iomem *SW_MUX_GPIO1_IO04;
static void __iomem *SW_PAD_GPIO1_IO04;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;
/* CSI_MCLK        ->LED_B     --GPIO4-19 */
static void __iomem *SW_MUX_GPIO4_IO19;
static void __iomem *SW_PAD_GPIO4_IO19;
static void __iomem *GPIO4_DR;
static void __iomem *GPIO4_GDIR;
/* CSI_HSYNC       ->LED_G     --GPIO4-20 */
static void __iomem *SW_MUX_GPIO4_IO20;
static void __iomem *SW_PAD_GPIO4_IO20;


struct newchrled_dev{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
};

struct newchrled_dev newchrled;

void led_switch(u8 num, u8 sta)
{
    u32 vul = 0;
    if(sta == LEDOFF)
    {
        switch(num)
        {
            case 1:
                vul = readl(GPIO1_DR);
                vul |= 1<<4;
                writel(vul,GPIO1_DR); 
            break;
            case 2:
                vul = readl(GPIO4_DR);
                vul |= 1<<19;
                writel(vul,GPIO4_DR);
            break;
            case 3:
                vul = readl(GPIO4_DR);
                vul |= 1<<20;
                writel(vul,GPIO4_DR);
            break;
        }
    }
    else
    {
        switch(num)
        {
            case 1:
                vul = readl(GPIO1_DR);
                vul &= ~(1<<4);
                writel(vul,GPIO1_DR); 
            break;
            case 2:
                vul = readl(GPIO4_DR);
                vul &= ~(1<<19);
                writel(vul,GPIO4_DR);
            break;
            case 3:
                vul = readl(GPIO4_DR);
                vul &= ~(1<<20);
                writel(vul,GPIO4_DR);
            break;
        }
    }
}


static ssize_t newled_read (struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}
static size_t newled_write (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    ret = copy_from_user(writebuf, buf, cnt);
    if(ret == 0)
    {
        led_switch(writebuf[0],writebuf[1]);
    }
    else
    {
        printk("kernel rev failed!!!\r\n");
    }
    return 0;
}
static int newled_open (struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;
    return 0;
}

static int newled_release (struct inode *inode, struct file *filp)
{
    //printk("chrdev_release!!!\r\n");
    return 0;
}
static struct file_operations newled_fops = {
    .owner = THIS_MODULE,
    .open = newled_open,
    .read = newled_read,
    .write = newled_write,
    .release = newled_release,
};
/*
SW_MUX_GPIO1_IO04;
SW_PAD_GPIO1_IO04;
SW_MUX_GPIO4_IO19;
SW_PAD_GPIO4_IO19;
SW_MUX_GPIO4_IO20;
SW_PAD_GPIO4_IO20;
*/
static int __init newled_init(void)
{
    u32 vul = 0;

    IMX6U_CCM_CCGR1     = ioremap(CCM_CCGR1_BASE, 4);
    IMX6U_CCM_CCGR3     = ioremap(CCM_CCGR3_BASE, 4);
/* GPIO_4          ->LED_R     --GPIO1-04 */   
    SW_MUX_GPIO1_IO04   = ioremap(SW_MUX_GPIO1_IO04_BASE, 4);
    SW_PAD_GPIO1_IO04   = ioremap(SW_PAD_GPIO1_IO04_BASE, 4);
    GPIO1_DR            = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR          = ioremap(GPIO1_GDIR_BASE, 4);

/* CSI_MCLK        ->LED_B     --GPIO4-19 */
    SW_MUX_GPIO4_IO19   = ioremap(SW_MUX_GPIO4_IO19_BASE, 4);
    SW_PAD_GPIO4_IO19   = ioremap(SW_PAD_GPIO4_IO19_BASE, 4);
    GPIO4_DR            = ioremap(GPIO4_DR_BASE, 4);  
    GPIO4_GDIR          = ioremap(GPIO4_GDIR_BASE, 4);
/* CSI_HSYNC       ->LED_G     --GPIO4-20 */
    SW_MUX_GPIO4_IO20   = ioremap(SW_MUX_GPIO4_IO20_BASE, 4);
    SW_PAD_GPIO4_IO20   = ioremap(SW_PAD_GPIO4_IO20_BASE, 4);

/* 使能时钟  */
    vul = readl(IMX6U_CCM_CCGR1);
    vul &= ~(3<<26);//
    vul |= 3<<26;//开启GPIO1时钟
    writel(vul,IMX6U_CCM_CCGR1);

    vul = readl(IMX6U_CCM_CCGR3);
    vul &= ~(3<<12);//
    vul |= 3<<12;//开启GPIO4时钟
    writel(vul,IMX6U_CCM_CCGR3);
/* 设置IO复用功能  */
    writel(5,SW_MUX_GPIO1_IO04);
    writel(0x10b0,SW_PAD_GPIO1_IO04);

    writel(5,SW_MUX_GPIO4_IO19);
    writel(0x10b0,SW_PAD_GPIO4_IO19);

    writel(5,SW_MUX_GPIO4_IO20);
    writel(0x10b0,SW_PAD_GPIO4_IO20);
/* 设置输出 */
    vul = readl(GPIO1_GDIR);
    vul |= 1<<4;
    writel(vul,GPIO1_GDIR);

    vul = readl(GPIO4_GDIR);
    vul |= 3<<19;
    writel(vul,GPIO4_GDIR);

/* 默认关闭LED */
    vul = readl(GPIO1_DR);
    vul &= ~(1<<4);//
    vul |= 1<<4;
    writel(vul,GPIO1_DR);

    vul = readl(GPIO4_DR);
    vul &= ~(3<<19);
    vul |= 3<<19;
    writel(vul,GPIO4_DR);

    if(newchrled.major)
    {
        newchrled.devid = MKDEV(newchrled.major,0);
        register_chrdev_region(newchrled.devid,NEWLED_CNT,NEWLEN_NAME);
    }
    else
    {
        alloc_chrdev_region(&newchrled.devid,0,NEWLED_CNT,NEWLEN_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    printk("newchrled major=%d,minor=%d\r\n",newchrled.major,newchrled.minor);

    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &newled_fops);
    cdev_add(&newchrled.cdev,newchrled.devid,NEWLED_CNT);

    newchrled.class = class_create(THIS_MODULE,NEWLEN_NAME);
    if(IS_ERR(newchrled.class))
    {
        return PTR_ERR(newchrled.class);
    }

    newchrled.device = device_create(newchrled.class,NULL,newchrled.devid,NULL,NEWLEN_NAME);
    if(IS_ERR(newchrled.device))
    {
        return PTR_ERR(newchrled.device);
    }
    
    return 0;
}

static void __exit newled_exit(void)
{
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(IMX6U_CCM_CCGR3);
        
    iounmap(SW_MUX_GPIO1_IO04);
    iounmap(SW_PAD_GPIO1_IO04);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    
    iounmap(SW_MUX_GPIO4_IO19);
    iounmap(SW_PAD_GPIO4_IO19);
    iounmap(GPIO4_DR);  
    iounmap(GPIO4_GDIR);
    
    iounmap(SW_MUX_GPIO4_IO20);
    iounmap(SW_PAD_GPIO4_IO20);
    
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid,NEWLED_CNT);
    device_destroy(newchrled.class,newchrled.devid);
    class_destroy(newchrled.class);

    printk("chrdev driver exit!!!\r\n");
}

module_init(newled_init);
module_exit(newled_exit);

MODULE_LICENSE("GPL");
