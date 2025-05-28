#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <stdio.h>

#include "kalman.h"
#include "ultrasonic.h"

#define DEBUG_INFO 0
 
#define ADV_INTERVAL_MS 200

/* ZEPHYR VARIABLES */
#define ADV_THREAD_STACK_SIZE 1024
#define ADV_THREAD_PRIORITY 7

struct hcsr04_fixture {
    const struct device *dev;
};

#define HCSR04_1_NODE DT_ALIAS(hcsr041)
#define HCSR04_2_NODE DT_ALIAS(hcsr04)
 
void ultrasonic();

static void update_adv_data(struct hcsr04_fixture fixture, int flag) 
{
    int err;

	struct kalman_values tx_data;

    /* Fetch sensor sample. */
    struct sensor_value value;
    sensor_sample_fetch(fixture.dev);
    sensor_channel_get(fixture.dev, SENSOR_CHAN_DISTANCE, &value);
    
    /* Report values. Send any measurements if they fit within the protocol. */
    /* Else, wait for the next advertisement interval. */
    //printk("Measured distance: %d.%06d meters\n", value.val1, value.val2);

    size_t size = sizeof(struct us_values);
    char *mem_ptr = k_malloc(size);

    double double_value = sensor_value_to_double(&value);
    if (flag == 1) {
        tx_data.x = double_value;
        tx_data.y = 0;
        tx_data.vx = 0;
        tx_data.vy = 0;
        tx_data.sensor = flag;
    } else {
        tx_data.x = 0;
        tx_data.y = double_value;
        tx_data.vx = 0;
        tx_data.vy = 0;
        tx_data.sensor = flag;
    }

    memcpy(mem_ptr, &tx_data, size);

    if (flag == 1){
        k_fifo_put(&kalman_us_fifo, mem_ptr);	
    } else {
        k_fifo_put(&kalman_rs_fifo, mem_ptr);
    }
    //k_free(mem_ptr);			
}

void ultrasonic() {
    static struct hcsr04_fixture fixture = {
        .dev = DEVICE_DT_GET(HCSR04_1_NODE),
    };

    static struct hcsr04_fixture fixture2 = {
        .dev = DEVICE_DT_GET(HCSR04_2_NODE),
    };

    while (1) {
        // Send LE BT advertisement at a fixed interval.
        update_adv_data(fixture, 1);
        k_sleep(K_MSEC(ADV_INTERVAL_MS));
        update_adv_data(fixture2, 2);
        k_sleep(K_MSEC(ADV_INTERVAL_MS));
    }
}