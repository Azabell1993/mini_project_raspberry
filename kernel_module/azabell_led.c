// /usr/src/raspberrypi_linux/drivers/char/azabell_led.c

#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/syscalls.h>

// #define GPIO_LED 589 // BCM_GPIO #18
#define GPIO_LED 18 // BCM GPIO #18

// 시스템 콜 핸들러
SYSCALL_DEFINE2(gpioled_control, int, pin, int, value) {
    if (pin == GPIO_LED) {
        if (value == 1) {
            gpio_set_value(GPIO_LED, 1); // LED ON
            printk(KERN_INFO "LED ON\n");
        } else {
            gpio_set_value(GPIO_LED, 0); // LED OFF
            printk(KERN_INFO "LED OFF\n");
        }
    } else {
        printk(KERN_WARNING "Invalid GPIO pin\n");
        return -EINVAL;
    }
    return 0;
}

static int __init gpioled_init(void) {
    int ret;
    ret = gpio_request(GPIO_LED, "gpioled");
    if (ret) {
        printk(KERN_ERR "Failed to request GPIO\n");
        return ret;
    }

    gpio_direction_output(GPIO_LED, 0); // 초기 LED 꺼짐 상태 설정
    printk(KERN_INFO "gpioled module loaded\n");
    return 0;
}

static void __exit gpioled_exit(void) {
    gpio_set_value(GPIO_LED, 0); // 모듈 언로드 시 LED OFF
    gpio_free(GPIO_LED);
    printk(KERN_INFO "gpioled module unloaded\n");
}

module_init(gpioled_init);
module_exit(gpioled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Azabell");
MODULE_DESCRIPTION("GPIO LED Control via System Call");

