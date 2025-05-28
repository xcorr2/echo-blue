#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "haptics.h"

int main(void)
{
    printk("M5Stack Core2 Haptics Test\n");

    haptics_init();

    haptics_start_strength(1);  /* Start at low strength */
    
    /* Gradually increase strength */
    for (int runs = 0; runs < 5; runs ++) {
        for (int strength = 1; strength <= 100; strength += 1) {
            printk("  Changing to %d%% strength\n", strength);
            haptics_set_strength(strength);
            k_sleep(K_MSEC(10));
        }
    }
    
    printk("  Ramping down strength\n");
    for (int strength = 90; strength >= 10; strength -= 10) {
        haptics_set_strength(strength);
        k_sleep(K_MSEC(100));
    }
    
    haptics_stop();
    k_sleep(K_MSEC(500));
    
    printk("Test 3: Pulsing effect\n");
    haptics_start_strength(30);
    
    for (int pulse = 0; pulse < 5; pulse++) {
        haptics_set_strength(100);  /* Strong pulse */
        k_sleep(K_MSEC(100));
        haptics_set_strength(20);   /* Weak background */
        k_sleep(K_MSEC(200));
    }
    
    haptics_stop();
    
    printk("Haptics test completed\n");
    
    return 0;
}