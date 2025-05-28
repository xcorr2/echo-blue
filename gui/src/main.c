#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>

#include "display.h"
#include "haptics.h"

#define GUI_TEST_THREAD_PRIORITY 7
#define TEST_THREAD_STACK_SIZE 1024

int main(void) {
    int ret;
    printk("Starting display test...\n");
    ret = display_init();
    printk("Display initialisation exited with: %d\n", ret);
    ret = haptics_init();
    printk("Haptics initialisation exited with: %d\n", ret);
    
}

/* Test thread. */
int gui_test_thread(void) {

    // wait for inits..
    k_sleep(K_SECONDS(20));

    printk("GUI Thread Starting...\n");
    int cycle = 0;
    
    int count = 0;
    while (count < 20) {
        haptics_start();
        trig_emergency_warning(cycle);
        for (int strength = 1; strength <= 100; strength += 1) {
            haptics_set_strength(strength);
            k_sleep(K_MSEC(4));
        }
        for (int strength = 100; strength >= 1; strength -= 1) {
            haptics_set_strength(strength);
            k_sleep(K_MSEC(4));
        }
        haptics_stop();
        k_sleep(K_MSEC(100));
        cycle = !cycle;
        count++;
    }
    
    return 0;
}

/* Define threads with respective priority. */
//REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/basic/threads/
K_THREAD_DEFINE(gui_test_thread_id, TEST_THREAD_STACK_SIZE,
    gui_test_thread, NULL, NULL, NULL,
    GUI_TEST_THREAD_PRIORITY, 0, 0);