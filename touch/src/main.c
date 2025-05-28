#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>


static void touch_input_callback(struct input_event *evt, void *user_data)
{
    static int current_x = 0;
    static int current_y = 0;

    switch (evt->code) {
    case INPUT_ABS_X:
        current_x = evt->value;
        break;
    case INPUT_ABS_Y:
        current_y = evt->value;
        break;
    case INPUT_BTN_TOUCH:
        if (evt->value == 1) {
            printk("Touch PRESSED at (%d, %d)\n", current_x, current_y);
        } else {
            printk("Touch RELEASED at (%d, %d)\n", current_x, current_y);
        }
        break;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(ft5336_touch)), 
                      touch_input_callback, NULL);

int main(void)
{
    const struct device *touch_dev;

    printk("M5 Core 2 Touch Test Ready\n");

    /* Verify touchscreen device */
    touch_dev = DEVICE_DT_GET(DT_NODELABEL(ft5336_touch));
    if (!device_is_ready(touch_dev)) {
        printk("Touch device not ready\n");
        return -1;
    }

    /* Main loop - keep application running */
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}