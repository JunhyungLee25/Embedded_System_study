#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Super User Teacher");
MODULE_DESCRIPTION("GPIO Button Driver for Up/Down Counter");

#define DRIVER_NAME "my_button"
#define DRIVER_CLASS "MyModulesClass_btn"

static dev_t btn_dev;
static struct class *btn_class;
static struct cdev btn_cdev;

// 버튼 GPIO 핀 정의 (23: Up, 22: Down)
static unsigned int btn_gpios[] = {23, 22};
static const char *btn_names[] = {"BTN_UP", "BTN_DOWN"};

/**
 * read 시스템 콜: 현재 버튼의 눌림 상태를 1바이트 데이터로 전송
 */
static ssize_t btn_read(struct file *File, char __user *user_buffer, size_t count, loff_t *offs) {
    unsigned char btn_state = 0;
    int i;

    // 각 GPIO 핀의 값을 읽어 비트 단위로 저장
    for (i = 0; i < 2; i++) {
        // gpio_get_value가 1이면 눌리지 않음, 0이면 눌림 (Pull-up 기준)
        // 여기서는 눌렸을 때 해당 비트를 1로 설정
        if (gpio_get_value(btn_gpios[i]) == 0) {
            btn_state |= (1 << i);
        }
    }

    if (copy_to_user(user_buffer, &btn_state, sizeof(btn_state))) return -EFAULT;
    return sizeof(btn_state);
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = btn_read,
};

static int __init btn_init(void) {
    int i;
    if (alloc_chrdev_region(&btn_dev, 0, 1, DRIVER_NAME) < 0) return -1;
    if ((btn_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) goto ClassError;
    if (device_create(btn_class, NULL, btn_dev, NULL, DRIVER_NAME) == NULL) goto FileError;

    cdev_init(&btn_cdev, &fops);
    if (cdev_add(&btn_cdev, btn_dev, 1) < 0) goto AddError;

    // GPIO 설정: 입력 모드로 설정
    for (i = 0; i < 2; i++) {
        if (gpio_request(btn_gpios[i], btn_names[i])) goto GpioError;
        gpio_direction_input(btn_gpios[i]); // 입력 방향 설정
    }
    return 0;

GpioError:
    while (i--) gpio_free(btn_gpios[i]);
AddError: device_destroy(btn_class, btn_dev);
FileError: class_destroy(btn_class);
ClassError: unregister_chrdev_region(btn_dev, 1);
    return -1;
}

static void __exit btn_exit(void) {
    int i;
    for (i = 0; i < 2; i++) gpio_free(btn_gpios[i]);
    cdev_del(&btn_cdev);
    device_destroy(btn_class, btn_dev);
    class_destroy(btn_class);
    unregister_chrdev_region(btn_dev, 1);
}

module_init(btn_init);
module_exit(btn_exit);