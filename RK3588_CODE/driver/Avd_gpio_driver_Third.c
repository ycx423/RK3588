/**
 * @file New_GPIO.c
 * @brief RK3588 GPIO控制驱动程序
 * @author Your Name
 * @date 2024
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>

// GPIO编号：A对应1，B对应2，C对应3，D对应4
// 在rk3588上确定GPIO编号的公式为：GPIOn_xy=n*32+(x-1)*8+y
// 因为选择的引脚为GPIO0_A0，所以通过n*32+(x-1)*8+y计算得到的编号为0。
#define DEVICE_NAME "GPIO_device"
#define GPIO_IOC_MAGIC 'k'
#define SET_GPIO_ON _IO(GPIO_IOC_MAGIC, 0)
#define SET_GPIO_OFF _IO(GPIO_IOC_MAGIC, 1)
#define SET_GPIO_ALL_ON _IO(GPIO_IOC_MAGIC, 2)
#define SET_GPIO_ALL_OFF _IO(GPIO_IOC_MAGIC, 3)
/*
A: B5 B4 A2
B: B0 C3 D3
C: B6 A6 A4
D: A5 B1 A7
*/
// GPIO引脚数组定义
#define MAX_GPIO_PINS 8
// 索引宏定义，便于用名字访问gpio_pins数组
#define IDX_GPIO3_B5 0
#define IDX_GPIO3_B4 1
#define IDX_GPIO3_A2 2

#define IDX_GPIO3_B0 3
#define IDX_GPIO3_C3 4
#define IDX_GPIO3_D3 5

#define IDX_GPIO3_B6 6
#define IDX_GPIO3_A6 7
#define IDX_GPIO3_A4 8

#define IDX_GPIO3_A5 9
#define IDX_GPIO3_B1 10
#define IDX_GPIO3_A7 11

static int gpio_pins[] = {
    109, // GPIO3_B5
    108, // GPIO3_B4
    98,  // GPIO3_A2

    104, // GPIO3_B0
    115, // GPIO3_C3
    123, // GPIO3_D3

    110, // GPIO3_B6
    102, // GPIO3_A6
    100, // GPIO3_A4

    101, // GPIO3_A5
    105, // GPIO3_B1
    103  // GPIO3_A7
};
static int gpio_count = ARRAY_SIZE(gpio_pins);

// 全局变量
static dev_t dev_num;
struct cdev my_cdev;
int major;
int minor;
static struct class *my_GPIO;
static struct device *GPIO_Device;
static int device_open(struct inode *inode, struct file *file)
{
    int i;
    // 初始化所有GPIO为输出模式，默认低电平
    for (i = 0; i < gpio_count; i++)
    {
        gpio_direction_output(gpio_pins[i], 0);
    }
    printk(KERN_INFO "GPIO Driver: Device opened, %d GPIOs initialized\n", gpio_count);
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    int i;
    // 设备关闭时将所有GPIO设为低电平
    for (i = 0; i < gpio_count; i++)
    {
        gpio_set_value(gpio_pins[i], 0);
    }
    printk(KERN_INFO "GPIO Driver: Device closed, all GPIOs set to LOW\n");
    return 0;
}

static long GPIO_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int i;

    switch (cmd)
    {
    case SET_GPIO_ON:
        // 设置指定GPIO为高电平，arg为GPIO数组索引
        if (arg >= gpio_count)
        {
            printk(KERN_WARNING "GPIO Driver: Invalid GPIO index: %lu,Max Number is %d\n", arg, gpio_count);
            return -EINVAL;
        }
        gpio_set_value(gpio_pins[arg], 1);
        printk(KERN_INFO "GPIO Driver: GPIO[%lu] ON (GPIO %d → HIGH)\n", arg, gpio_pins[arg]);
        break;

    case SET_GPIO_OFF:
        // 设置指定GPIO为低电平，arg为GPIO数组索引
        if (arg >= gpio_count)
        {
            printk(KERN_WARNING "GPIO Driver: Invalid GPIO index: %lu\n", arg);
            return -EINVAL;
        }
        gpio_set_value(gpio_pins[arg], 0);
        printk(KERN_INFO "GPIO Driver: GPIO[%lu] OFF (GPIO %d → LOW)\n", arg, gpio_pins[arg]);
        break;

    case SET_GPIO_ALL_ON:
        // 设置所有GPIO为高电平
        for (i = 0; i < gpio_count; i++)
        {
            gpio_set_value(gpio_pins[i], 1);
        }
        printk(KERN_INFO "GPIO Driver: All GPIOs set to HIGH\n");
        break;

    case SET_GPIO_ALL_OFF:
        // 设置所有GPIO为低电平
        for (i = 0; i < gpio_count; i++)
        {
            gpio_set_value(gpio_pins[i], 0);
        }
        printk(KERN_INFO "GPIO Driver: All GPIOs set to LOW\n");
        break;

    default:
        printk(KERN_WARNING "GPIO Driver: Invalid ioctl command: 0x%08x\n", cmd);
        return -ENOTTY;
    }

    return 0;
}

// 文件操作结构体，连接用户空间操作与内核函数
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = GPIO_ioctl,
};

/**
 * @brief 驱动程序初始化函数
 * @return 0表示成功，负数表示错误码
 */
static int __init mydevice_init(void)
{
    int ret;
    int i;

    printk(KERN_INFO "GPIO Driver: Initializing driver...\n");

    // 申请所有GPIO资源
    for (i = 0; i < gpio_count; i++)
    {
        // 先释放可能已占用的GPIO
        gpio_free(gpio_pins[i]);

        // 申请GPIO资源
        if (gpio_request(gpio_pins[i], "gpio_control")) // 返回值 成功为 0 失败为 1
        {
            printk(KERN_ERR "GPIO Driver: Failed to request GPIO %d\n", gpio_pins[i]);
            // 失败时释放已申请的GPIO
            while (--i >= 0)
            {
                gpio_free(gpio_pins[i]);
            }
            return -EBUSY;
        }
        printk(KERN_INFO "GPIO Driver: GPIO %d requested successfully\n", gpio_pins[i]);
    }

    // 动态分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        printk(KERN_ERR "GPIO Driver: Failed to allocate device number: %d\n", ret);
        return ret;
    }
    // // 修改后：完整的资源清理 添加的资源清理///////////////////////////////////////////
    // if (ret != 0)
    // {
    //     printk(KERN_ERR "GPIO Driver: Failed to request GPIO %d, error: %d\n", gpio_pins[i], ret);
    //     // 失败时释放已申请的GPIO
    //     while (--i >= 0)
    //     {
    //         gpio_free(gpio_pins[i]);
    //     }
    //     return -EBUSY;
    // }
    /////////////////////////////////////////////////////////////
    major = MAJOR(dev_num);
    minor = MINOR(dev_num);
    printk(KERN_INFO "GPIO Driver: Allocated device number - Major: %d, Minor: %d\n", major, minor);

    // 注册字符设备
    my_cdev.owner = THIS_MODULE;
    cdev_init(&my_cdev, &fops);
    cdev_add(&my_cdev, dev_num, 1);

    printk(KERN_INFO "GPIO Driver: Character device registered successfully\n");

    // 创建设备类
    my_GPIO = class_create(THIS_MODULE, "my_GPIO");
    if (IS_ERR(my_GPIO))
    {
        pr_err("Failed to create class\n");
        return PTR_ERR(my_GPIO);
    }

    // 创建设备节点
    GPIO_Device = device_create(my_GPIO, NULL, MKDEV(major, minor), NULL, "GPIO_Device");
    if (IS_ERR(GPIO_Device))
    {
        pr_err("Failed to create device\n");
        class_destroy(my_GPIO);
        return PTR_ERR(GPIO_Device);
    }

    printk(KERN_INFO "GPIO Driver: Device node '/dev/GPIO_Device' created successfully\n");
    printk(KERN_INFO "GPIO Driver: Driver initialization completed successfully!\n");

    // 打印成功开启的GPIO端口信息
    printk(KERN_INFO "GPIO Driver: Successfully enabled GPIO ports: ");
    for (i = 0; i < gpio_count; i++)
    {
        if (i == gpio_count - 1)
            printk(KERN_CONT "%d\n", gpio_pins[i]); // 最后一个GPIO，换行
        else
            printk(KERN_CONT "%d, ", gpio_pins[i]); // 中间的GPIO，用逗号分隔
    }

    return 0;
}

/**
 * @brief 驱动程序退出函数
 */
static void __exit mydevice_exit(void)
{
    int i;

    printk(KERN_INFO "GPIO Driver: Starting driver cleanup...\n");

    // 释放所有GPIO资源
    for (i = 0; i < gpio_count; i++)
    {
        gpio_free(gpio_pins[i]);
        printk(KERN_INFO "GPIO Driver: GPIO %d released\n", gpio_pins[i]);
    }

    // 销毁设备节点
    device_destroy(my_GPIO, MKDEV(major, minor));
    printk(KERN_INFO "GPIO Driver: Device node '/dev/GPIO_Device' destroyed\n");

    // 销毁设备类
    class_destroy(my_GPIO);
    printk(KERN_INFO "GPIO Driver: Device class 'my_GPIO' destroyed\n");

    // 删除字符设备
    cdev_del(&my_cdev);
    printk(KERN_INFO "GPIO Driver: Character device deleted\n");

    // 注销设备号
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "GPIO Driver: Device number unregistered (Major: %d)\n", major);

    printk(KERN_INFO "GPIO Driver: Driver cleanup completed successfully!\n");
}

module_init(mydevice_init);
module_exit(mydevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple character device driver");
