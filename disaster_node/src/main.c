/*
 * Copyright (c) 2018 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#ifndef IBEACON_RSSI
#define IBEACON_RSSI 0xc8
#endif

#define MY_STACK_SIZE 800
#define MY_PRIORITY 5

// thread memory space
K_THREAD_STACK_DEFINE(stack_area, 1024*2);
K_THREAD_STACK_DEFINE(stack_area1, 1024*3);
K_THREAD_STACK_DEFINE(stack_area2, 1024*2);
K_THREAD_STACK_DEFINE(stack_area3, 1024*3);

// threads and thread ids
struct k_thread observer_thread;
struct k_thread rssi_thread;
struct k_thread kalman_thread;
struct k_thread us_thread;
struct k_thread gui_thread;


k_tid_t observer_tid;
k_tid_t rssi_tid;
k_tid_t kalman_tid;
k_tid_t us_tid;
k_tid_t gui_tid;

struct k_fifo ble_fifo;

#include "ultrasonic.h"
#include "kalman.h"
#include "gps.h"

#define OFFSET 5

/*
 * Set iBeacon demo advertisement data. These values are for
 * demonstration only and must be changed for production environments!
 *
 * UUID:  18ee1516-016b-4bec-ad96-bcb96d166e97
 * Major: 0
 * Minor: 0
 * RSSI:  -56 dBm
 */
 /* Mutable portion of advert. */
 static uint8_t ad_payload[25] = {
    0x95, 0x12,                        // Manufactuer ID: 0x0059 (Nordic Semi)
    0x02, 0x15,               // Custom ID.
    0x01,                              // Sequence counter. 
    0xF4, 0x01, 0x04, 0x05, 0x06,      // PADDING
    0x08, 0x07, 0x06      ,            // ULTRASONIC VALUES (meters, millimeter high, millimeter low)
    0x00, 0x00, 0x00, 0x00, 0x00,       // PADDING
    0xFF, 00, 00, 00, 00, 00, 00                               // Extra ID byte.
};

/* Immutable portion of advert. */
// REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/bluetooth/iBeacon/
static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, ad_payload, sizeof(ad_payload))
};

struct gps_values {
    void *fifo_reserved;
    double gps_values[4];
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("iBeacon started\n");
}

#define MAX_BCD_DIGITS 10  // Total number of digits (before + after decimal)

typedef struct {
    uint8_t bcd[(MAX_BCD_DIGITS + 1) / 2];  // 2 digits per byte
    int len;                                // number of BCD bytes used
} BCDResult;

BCDResult double_to_bcd(double num) {
    BCDResult result = { .bcd = {0}, .len = 0 };

    if (num < 0) num = -num;

    uint64_t int_part = (uint64_t)num;
    double frac_part = num - int_part;

    char str[32];
    int idx = 0;

    // Convert integer part to string (in reverse, then correct)
    if (int_part == 0) {
        str[idx++] = '0';
    } else {
        char temp[20];
        int temp_idx = 0;
        while (int_part > 0) {
            temp[temp_idx++] = '0' + (int_part % 10);
            int_part /= 10;
        }
        for (int i = temp_idx - 1; i >= 0; i--) {
            str[idx++] = temp[i];
        }
    }

    // Convert fractional part to up to 9 digits
    for (int i = 0; i < 9; i++) {
        frac_part *= 10.0;
        int digit = (int)frac_part;
        str[idx++] = '0' + digit;
        frac_part -= digit;
    }

    str[idx] = '\0';

    // Convert string to BCD
    int j = 0;
    for (int i = 0; i < idx && j < MAX_BCD_DIGITS; i++) {
        uint8_t digit = str[i] - '0';
        if (j % 2 == 0) {
            result.bcd[j / 2] = (digit << 4);
        } else {
            result.bcd[j / 2] |= digit;
        }
        j++;
    }
    
    result.len = (j + 1) / 2;
    return result;
}

static void update_adv_data(struct gps_values* rx_data) {
    double num = rx_data->gps_values[0];
    uint8_t* bcd_array;

    BCDResult a = double_to_bcd(num);

    for (int i = OFFSET; i < a.len + OFFSET; i++) {
        ad_payload[i] = (uint8_t)a.bcd[i-OFFSET];
    }

    double num2 = rx_data->gps_values[1];
    uint8_t* bcd_array2;

    BCDResult b = double_to_bcd(num2);

    for (int j = OFFSET+a.len; j < b.len + OFFSET + a.len-1; j++) {
        ad_payload[j] = (uint8_t)b.bcd[j-OFFSET-a.len];
    }
    printf("success\n");

    /* Restart advertising. */
    bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    k_free(rx_data);
    /*k_msleep(1000);
    k_msleep(1000);
    bt_le_adv_stop();    

    while(1){
        k_msleep(100000);
    }*/
}

int main(void)
{
	int err;
	printk("Starting iBeacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
    bt_le_adv_stop();    

    k_fifo_init(&kalman_us_fifo);
	k_fifo_init(&kalman_rs_fifo);
	k_fifo_init(&ble_fifo);
    k_sem_init(&signal, 0, 1);

    //k_fifo_init(&signal_fifo);

	kalman_tid = k_thread_create(&kalman_thread, stack_area2,
	    K_THREAD_STACK_SIZEOF(stack_area2), create_filter,
	    NULL, NULL, NULL, MY_PRIORITY, 0, K_NO_WAIT);
	// handle ultra sonic sensor processing
	us_tid = k_thread_create(&us_thread, stack_area3,
	    K_THREAD_STACK_SIZEOF(stack_area3), ultrasonic,
	    NULL, NULL, NULL, MY_PRIORITY, 0, K_NO_WAIT);

    struct gps_values* rx_data;
    int flag = 0;
    while(1){
        //if (1) {
        if (!k_fifo_is_empty(&ble_fifo)){
            // data recieved, limit any values to not be read if outside grid
            rx_data = k_fifo_get(&ble_fifo, K_FOREVER);
            update_adv_data(rx_data);
        }
        k_msleep(500);
    }    
	return 0;
}

/* Test thread. */
int gps_test_thread(void) 
{

    struct GNGGA_GPS_data GNGGA_data;
    struct GNRMC_GPS_data GNRMC_data;

    double latitude, longitude; 
    int status;
 
    k_msleep(1000);
    
    while (1) {

        // EXAMPLE USEAGE
        // RETURN VALUES:
        // status = 0 - no gps sources
        // status = 1 - 1 gps source
        // status = 2 - 2 gps sources

        //status = get_gps_data(&latitude, &longitude);

        printf("GPS Coordinates (%d Sources): %.6f, %.6f\n", 
            status, latitude, longitude);
        if (k_sem_take(&signal, K_MSEC(50)) != 0) {
            printk("Input data not available!");
        } else {
        /* fetch available data */
        
        struct gps_values tx_data;
        tx_data.gps_values[0] = 27.500685;
        tx_data.gps_values[1] = 153.015555;

        size_t size = sizeof(struct gps_values);
        char *mem_ptr = k_malloc(size);
        memcpy(mem_ptr, &tx_data, size);
        k_fifo_put(&ble_fifo, mem_ptr);
        }


        k_msleep(1000);
    }

    return 0;
}

#define GPS_TEST_THREAD_PRIORITY 5
#define TEST_THREAD_STACK_SIZE 1024*4

/* Define threads with respective priority. */
//REF: https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/basic/threads/
K_THREAD_DEFINE(gps_test_thread_id, TEST_THREAD_STACK_SIZE,
    gps_test_thread, NULL, NULL, NULL,
    GPS_TEST_THREAD_PRIORITY, 0, 0);

//K_THREAD_DEFINE(gps_thread_id, GPS_THREAD_STACK_SIZE,
//    GPS_thread, NULL, NULL, NULL,
//    GPS_THREAD_PRIORITY, 0, 0);

//K_THREAD_DEFINE(adv_thread_id, ADV_THREAD_STACK_SIZE,
//    adv_thread, NULL, NULL, NULL,
//    ADV_THREAD_PRIORITY, 0, 0);
