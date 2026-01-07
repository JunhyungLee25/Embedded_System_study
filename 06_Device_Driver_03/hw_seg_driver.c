#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Super User Teacher");
MODULE_DESCRIPTION("7-Segment Display Driver");

#define DRIVER_NAME "my_segment"
#define DRIVER_CLASS "MyModulesClass_seg"

static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

// GPIO 핀 정의 (자릿수 4개, 세그먼트 8개 순서)
static unsigned int seg_gpios[] = {2, 3, 4, 17, 21, 20, 16, 12, 7, 8, 25, 24};
static const char *seg_names[] = {"D1", "D2", "D3", "D4", "A", "B", "C", "D", "E", "F", "G", "DP"};

static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
    unsigned short value = 0;
    int i;

    // 유저 공간의 데이터를 커널 공간의 value 변수로 안전하게 복사
    if (copy_from_user(&value, user_buffer, sizeof(unsigned short))) return -EFAULT;

    // 12개 비트를 순회하며 각 GPIO에 0 or 1 적용 
    for (i = 0; i < 12; i++) {
        gpio_set_value(seg_gpios[i], (value >> i) & 0x01);
    }
    return sizeof(unsigned short);
}

static int driver_open(struct inode *device_file, struct file *instance) { return 0; }
static int driver_close(struct inode *device_file, struct file *instance) { return 0; }

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = driver_write
};

static int __init ModuleInit(void) {
    int i;
    if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) return -1;
    if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) goto ClassError;
    if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) goto FileError;

    cdev_init(&my_device, &fops);
    if (cdev_add(&my_device, my_device_nr, 1) < 0) goto AddError;

    // GPIO 일괄 요청 및 설정
    for (i = 0; i < 12; i++) {
        if (gpio_request(seg_gpios[i], seg_names[i])) goto GpioError;
        gpio_direction_output(seg_gpios[i], 0);
    }
    return 0;

GpioError:
    while (i--) gpio_free(seg_gpios[i]);
AddError: device_destroy(my_class, my_device_nr);
FileError: class_destroy(my_class);
ClassError: unregister_chrdev_region(my_device_nr, 1);
    return -1;
}

static void __exit ModuleExit(void) {
    int i;
    for (i = 0; i < 12; i++) {
        gpio_set_value(seg_gpios[i], 0);
        gpio_free(seg_gpios[i]);
    }
    cdev_del(&my_device);
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class);
    unregister_chrdev_region(my_device_nr, 1);
}

module_init(ModuleInit);
module_exit(ModuleExit);