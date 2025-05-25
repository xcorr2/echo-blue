/**
 * @file main.c
 * @brief Example application for XA1110 I2C GPS module
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include "gps.h"

int main() {
    printk("Starting GPS testing...\n");
    return 1;
}

/* Test thread. */
int gps_test_thread(void) {

    struct GNGGA_GPS_data GNGGA_data;
    struct GNRMC_GPS_data GNRMC_data;

    double latitude, longitude; 
    int status;

    while (1) {

        // EXAMPLE USEAGE
        // RETURN VALUES:
        // status = 0 - no gps sources
        // status = 1 - 1 gps source
        // status = 2 - 2 gps sources

        status = get_gps_data(&latitude, &longitude);

        printf("GPS Coordinates (%d Sources): %.6f, %.6f\n", 
            status, latitude, longitude);

        k_msleep(5000);
    }

    return 0;
}

#define GPS_TEST_THREAD_PRIORITY 7
#define TEST_THREAD_STACK_SIZE 1024

/* Define threads with respective priority. */
//REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/basic/threads/
K_THREAD_DEFINE(gps_test_thread_id, TEST_THREAD_STACK_SIZE,
    gps_test_thread, NULL, NULL, NULL,
    GPS_TEST_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(gps_thread_id, GPS_THREAD_STACK_SIZE,
    GPS_thread, NULL, NULL, NULL,
    GPS_THREAD_PRIORITY, 0, 0);

