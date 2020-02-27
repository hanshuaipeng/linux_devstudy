#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

#define CHRDEV_MAJOR    200
#define CHRDEV_NAME     "chardevbase"

static char readbuf[100];
static char writebuf[100];
static char kerneldata[] = {"kernel data dev!"};

static ssize_t chrdev_read (struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    memcpy(readbuf, kerneldata, sizeof(kerneldata));
    ret = copy_to_user(buf, readbuf, cnt);
    if(ret == 0)
    {
        printk("kernel senddata ok!!!\r\n");
    }
    else
    {
        printk("kernel senddata failed!!!\r\n");
    }
    
    return 0;
}
static size_t chrdev_write (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    ret = copy_from_user(writebuf, buf, cnt);
    if(ret == 0)
    {
        printk("kernel rev ok!!!\r\n");
    }
    else
    {
        printk("kernel rev failed!!!\r\n");
    }
    return 0;
}
static int chrdev_open (struct inode *inode, struct file *filp)
{
    printk("chrdev_open!!!\r\n");
    return 0;
}

static int chrdev_release (struct inode *inode, struct file *filp)
{
    //printk("chrdev_release!!!\r\n");
    return 0;
}
static struct file_operations chrdev_fops = {
    .owner = THIS_MODULE,
    .open = chrdev_open,
    .read = chrdev_read,
    .write = chrdev_write,
    .release = chrdev_release,
};


static int __init chrdev_init(void)
{
    int ret = 0;
    ret = register_chrdev(CHRDEV_MAJOR, CHRDEV_NAME,
				  &chrdev_fops);
    if(ret < 0)
    {
        printk("chrdev driver register fialed !!!\r\n");
    }
    printk("chrdev driver init!!!\r\n");
    return 0;
}

static void __exit chrdev_exit(void)
{
    unregister_chrdev(CHRDEV_MAJOR, CHRDEV_NAME);
    printk("chrdev driver exit!!!\r\n");
}

module_init(chrdev_init);
module_exit(chrdev_exit);

MODULE_LICENSE("GPL");
