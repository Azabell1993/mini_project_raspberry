// /usr/src/raspberrypi_linux/arch/arm64/kernel/sys.c

#include <linux/gpio.h>  // GPIO 제어 함수 사용을 위한 헤더 파일 추가
#include <linux/printk.h> // printk 함수 사용을 위해 필요


SYSCALL_DEFINE0(azabell)
{
	printk(KERN_INFO "azabell systemcall\n");

	return (0);
}

SYSCALL_DEFINE2(azabell_led, int, pin, int, value)
{
    printk(KERN_INFO "azabell_led system call is invoked with pin=%d and value=%d\n", pin, value);

    int ret;
    pin += 571;

    // GPIO 핀을 요청하여 사용할 준비
    ret = gpio_request(pin, "sys_azabell_led");
    if (ret) {
        printk(KERN_ERR "Failed to request GPIO %d\n", pin);
        return -EINVAL;
    }

    // GPIO 핀을 출력 모드로 설정
    ret = gpio_direction_output(pin, value);
    if (ret) {
        printk(KERN_ERR "Failed to set GPIO direction for pin %d\n", pin);
        gpio_free(pin);
        return -EINVAL;
    }

    // GPIO 값 설정
    gpio_set_value(pin, value);

    // 사용 후 GPIO 핀 해제
    gpio_free(pin);

    printk(KERN_INFO "GPIO %d set to %d\n", pin - 571, value);

    return 0;
}
