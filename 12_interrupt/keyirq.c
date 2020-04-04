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
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/platform_device.h>

#define IMXIRQ_CNT 		1               //设备号数量
#define IMXIRQ_NAMED    "key_interrupt" //设备名称
#define KEY0VALUE       0x01            //键值
#define INVAKEY         0xff            //无效的键值
#define KEY_NUM         1               //按键数量

struct irq_keydesc {
    int gpio;                           /* gpio */
    int irqnum;                         /* 中断号 */
    unsigned char value;                /* 按键对应的键值 */
    char name[10];                      /* 名字 */
    irqreturn_t (*handler)(int, void *); /* 中断服务函数 */
};

struct imx6ullirq_dev
{
    dev_t devid;                            /* 设备号 */
    struct cdev cdev;                       /* cdev */
    struct class *class;                    /* 类 */
    struct device *device;                  /* 设备 */
    int major;                              /* 主设备号 */
    int minor;                              /* 次设备号 */
    struct device_node *nd;                 /* 设备节点 */
    atomic_t keyvalue;                      /* 有效的按键键值 */
    atomic_t releasekey;                    /* 标记是否完成一次完成的按键*/
    struct timer_list timer;                /* 定义一个定时器*/
    struct irq_keydesc irqkeydesc[KEY_NUM]; /* 按键描述数组 */
    unsigned char curkeynum;                /* 当前的按键号 */
};

struct imx6ullirq_dev imx6uirq;

static irqreturn_t key0_handler(int irq, void *dev_id)
{
    struct imx6ullirq_dev *dev = (struct imx6ullirq_dev*)dev_id;
    dev->curkeynum = 0;
    dev->timer.data = (volatile long)dev_id;
    mod_timer(&dev->timer, jiffies+ msecs_to_jiffies(10));//10ms
    return IRQ_HANDLED;
}

void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keyesc;
    struct imx6ullirq_dev *dev = (struct imx6ullirq_dev *)arg;

    num = dev->curkeynum;
    keyesc = &dev->irqkeydesc[num];

    value = gpio_get_value(keyesc->gpio);
    if(value == 0)
    {
        atomic_set(&dev->keyvalue,keyesc->value);
    }
    else
    {
        atomic_set(&dev->keyvalue,0x80|keyesc->value);
        atomic_set(&dev->releasekey,1);
    }
    
}


static int keyio_init(void)
{
    unsigned char i = 0;
    char name[10];                      
    int ret = 0;
    imx6uirq.nd = of_find_node_by_path("/gpio_key");
    if(imx6uirq.nd == NULL)
    {
        printk("key node notr find!\r\n");
        return -EINVAL;
    }
    for(i=0; i<KEY_NUM; i++)
    {
        imx6uirq.irqkeydesc[i].gpio = of_get_named_gpio(imx6uirq.nd,"key-gpio",i);
        if(imx6uirq.irqkeydesc[i].gpio < 0)
        {
             printk("can't get key%d!\r\n",i);
        }
    }
    /* 初始化 key 所使用的 IO，并且设置成中断模式 */
    for(i=0; i<KEY_NUM; i++)
    {
        memset(imx6uirq.irqkeydesc[i].name,0,sizeof(imx6uirq.irqkeydesc[i].name));
        sprintf(imx6uirq.irqkeydesc[i].name, "KEY%d", i);
        gpio_request(imx6uirq.irqkeydesc[i].gpio ,name);
        imx6uirq.irqkeydesc[i].irqnum = irq_of_parse_and_map(imx6uirq.nd,i);
        printk("key%d:gpio=%d, irqnum=%d\r\n",i, imx6uirq.irqkeydesc[i].gpio, imx6uirq.irqkeydesc[i].irqnum);
    }
    imx6uirq.irqkeydesc[0].handler = key0_handler;
    imx6uirq.irqkeydesc[0].value = KEY0VALUE;
    for(i=0; i<KEY_NUM; i++)
    {
        ret = request_irq(imx6uirq.irqkeydesc[i].irqnum,imx6uirq.irqkeydesc[i].handler,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,imx6uirq.irqkeydesc[i].name, &imx6uirq);
        if(ret < 0)
        {
            printk("irq %d request failed!\r\n",imx6uirq.irqkeydesc[i].irqnum);
            return -EFAULT;
        }
    }

    init_timer(&imx6uirq.timer);
    imx6uirq.timer.function = timer_function;
    return 0;
}

static int imx6uirq_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &imx6uirq; /* 设置私有数据 */
    return 0;
}

static ssize_t imx6uirq_read(struct file *filp, char __user *buf,size_t cnt, loff_t *offt)
{
    int ret = 0;
    unsigned char keyvalue = 0;
    unsigned char releasekey = 0;
    struct imx6ullirq_dev *dev = (struct imx6ullirq_dev *)filp->private_data;

    keyvalue = atomic_read(&dev->keyvalue);
    releasekey = atomic_read(&dev->releasekey);

    if (releasekey) 
    { /* 有按键按下 */
        if (keyvalue & 0x80)
        {
            keyvalue &= ~0x80;
            ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
        } 
        else 
        {
            goto data_error;
        }
        atomic_set(&dev->releasekey, 0); /* 按下标志清零 */
    } 
    else 
    {
        goto data_error;
    }                                                               
    return 0;
data_error: 
    return -EINVAL;
}
static struct file_operations imx6uirq_ops = {
    .owner = THIS_MODULE,
    .open = imx6uirq_open,
    .read = imx6uirq_read,
};

static const struct of_device_id irq_of_match[] = {
    {.compatible = "key"},
    {}
};


int imx6uirq_probe(struct platform_device *dev)
{
    printk("irq driver and device was matched!\r\n");
    if (imx6uirq.major)
    {
        imx6uirq.devid = MKDEV(imx6uirq.major,0);
        register_chrdev_region(imx6uirq.devid,IMXIRQ_CNT,IMXIRQ_NAMED);
    }
    else
    {
        alloc_chrdev_region(&imx6uirq.devid,0,IMXIRQ_CNT,IMXIRQ_NAMED);
        imx6uirq.major = MAJOR(imx6uirq.devid);
        imx6uirq.minor = MINOR(imx6uirq.devid);
    }
    cdev_init(&imx6uirq.cdev,&imx6uirq_ops);
    cdev_add(&imx6uirq.cdev,imx6uirq.devid,IMXIRQ_CNT);
    
    imx6uirq.class = class_create(THIS_MODULE,IMXIRQ_NAMED);
    if(IS_ERR(imx6uirq.class))
    {
        return PTR_ERR(imx6uirq.class);
    }

    imx6uirq.device = device_create(imx6uirq.class,NULL,imx6uirq.devid,NULL,IMXIRQ_NAMED);
    if(IS_ERR(imx6uirq.device))
    {
        return PTR_ERR(imx6uirq.device);
    }

    /* 5、 初始化按键 */
    atomic_set(&imx6uirq.keyvalue, INVAKEY);
    atomic_set(&imx6uirq.releasekey, 0);
    keyio_init();
    return 0;
}
int imx6uirq_remove(struct platform_device *dev)
{
    unsigned int i = 0;
    del_timer_sync(&imx6uirq.timer);
    for(i=0;i<KEY_NUM; i++)
    {
        free_irq(imx6uirq.irqkeydesc[i].irqnum,&imx6uirq);
    }
    cdev_del(&imx6uirq.cdev);
    unregister_chrdev_region(imx6uirq.devid, IMXIRQ_CNT); /*注销设备号*/

	device_destroy(imx6uirq.class, imx6uirq.devid);
	class_destroy(imx6uirq.class);
    return 0 ;
}

static struct platform_driver imx6uirq_driver = {
    .driver = {
        .name = "imx6uirq",
        .of_match_table = irq_of_match,
    },
    .probe = imx6uirq_probe,
	.remove = imx6uirq_remove,
};

static int __init imx6uirq_init(void)
{
	return platform_driver_register(&imx6uirq_driver);
}

static void __exit imx6uirq_exit(void)
{
	platform_driver_unregister(&imx6uirq_driver);
}

module_init(imx6uirq_init);
module_exit(imx6uirq_exit);


MODULE_LICENSE("GPL");
