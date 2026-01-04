#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Y.B. Cho LiuWei");
MODULE_DESCRIPTION("GPIO Driver for LED and Button");

/* 가상 장치 파일 정보 */
#define DRIVER_NAME "my_gpio"
#define DRIVER_CLASS "my_gpio_class"

/* GPIO 핀 번호 설정 (라즈베리 파이 4B 기준) */
#define GPIO_LED 4      // LED 연결 핀
#define GPIO_BTN 17     // 버튼 연결 핀 

static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

/**
 * @brief 장치에서 데이터를 읽어올 때 (버튼 상태 확인) 
 */
static ssize_t driver_read(struct file *instance, char __user *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    char tmp;

    to_copy = min(count, sizeof(tmp));
    
    // 버튼 상태 읽기 (0 또는 1)
    tmp = gpio_get_value(GPIO_BTN) + '0'; 

    // 커널 데이터를 사용자 공간으로 복사 
    not_copied = copy_to_user(user_buffer, &tmp, to_copy);
    delta = to_copy - not_copied;

    return delta;
}

/**
 * @brief 장치에 데이터를 쓸 때 (LED 제어) 
 */
static ssize_t driver_write(struct file *instance, const char __user *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    char tmp;

    to_copy = min(count, sizeof(tmp));

    // 사용자 공간에서 데이터를 가져옴
    not_copied = copy_from_user(&tmp, user_buffer, to_copy);
    delta = to_copy - not_copied;

    // 전달받은 값에 따라 LED ON/OFF
    if (tmp == '1') {
        gpio_set_value(GPIO_LED, 1);
    } else if (tmp == '0') {
        gpio_set_value(GPIO_LED, 0);
    } else {
        printk("Invalid Input!\n");
    }

    return delta;
}

static int driver_open(struct inode *device_file, struct file *instance) {
    printk("led_button open was called!\n");
    return 0;
}

static int driver_close(struct inode *device_file, struct file *instance) {
    printk("led_button close was called!\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .read = driver_read,
    .write = driver_write
};

static int __init ModuleInit(void) {
    printk("Hello, Kernel!\n");

    /* 1. 장치 번호 할당 */
    if (alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
        printk("Device Nr. could not be allocated!\n");
        return -1;
    }

    /* 2. 장치 클래스 생성 */
    if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
        unregister_chrdev_region(my_device_nr, 1);
        return -1;
    }

    /* 3. 장치 노드 생성 */
    if (device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
        class_destroy(my_class);
        unregister_chrdev_region(my_device_nr, 1);
        return -1;
    }

    /* 4. 문자 장치 초기화 및 등록 */
    cdev_init(&my_device, &fops);
    if (cdev_add(&my_device, my_device_nr, 1) < 0) {
        device_destroy(my_class, my_device_nr);
        class_destroy(my_class);
        unregister_chrdev_region(my_device_nr, 1);
        return -1;
    }

    /* 5. GPIO 설정 */
    if (gpio_request(GPIO_LED, "rpi-gpio-4")) {
        printk("Can not allocate GPIO 4\n");
        return -1;
    }
    gpio_direction_output(GPIO_LED, 0);

    if (gpio_request(GPIO_BTN, "rpi-gpio-17")) {
        printk("Can not allocate GPIO 17\n");
        gpio_free(GPIO_LED);
        return -1;
    }
    gpio_direction_input(GPIO_BTN);

    return 0;
}

static void __exit ModuleExit(void) {
    gpio_free(GPIO_LED);
    gpio_free(GPIO_BTN); 
    cdev_del(&my_device); 
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class); 
    unregister_chrdev_region(my_device_nr, 1); 
    printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);