/**
 * @file gps.h
 * @brief Driver for SparkFun's XA1110 GPS I2C module
 *
 * This library handles communication with SparkFun's I2C GPS module (XA1110)
 * which uses the MediaTek MT3333 GPS chipset with special I2C firmware.
 */

#ifndef I2C_GPSH
#define I2C_GPSH

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#define MT333X_I2C_ADDR            0x10  /* 7-bit I2C address */
#define GPS_MAX_PACKET_SIZE        255   /* Max bytes to buffer from GPS */

#define GPS_SUCCESS                0
#define GPS_ERR_I2C_INIT          -1
#define GPS_ERR_NO_RESPONSE       -2
#define GPS_ERR_WRITE_FAIL        -3

#define GPS_THREAD_PRIORITY 5
#define GPS_THREAD_STACK_SIZE 4096*4

struct gps_i2c_data {
    uint8_t gps_buffer[GPS_MAX_PACKET_SIZE];
    uint8_t head;  
    uint8_t tail;  
    const struct device *i2c_dev;
    bool debug_enabled;
};

struct GNGGA_GPS_data {
    uint8_t lock;
    double timestamp;
    double latitude;
    char lat_direction;    // 'N' or 'S'
    double longitude;
    char lon_direction;    // 'E' or 'W'
    int fix_quality;       // 0 = invalid, 1 = GPS fix, 2 = DGPS fix
    int satellites_used;
};

struct GNRMC_GPS_data {
    uint8_t lock;
    double timestamp;
    double latitude;
    char lat_direction;    // 'N' or 'S'
    double longitude;
    char lon_direction;    // 'E' or 'W'
    double speed;      
    double course;
};

int gps_init(struct gps_i2c_data *data);
int gps_check(struct gps_i2c_data *data);
uint8_t gps_available(struct gps_i2c_data *data);
uint8_t gps_read(struct gps_i2c_data *data);
int gps_send_mtk_packet(struct gps_i2c_data *data, const char *command, size_t len);
int gps_create_mtk_packet(uint16_t packet_type, const char *data_field, 
                          char *buffer, size_t buffer_size);
int gps_calc_crc(const char *sentence, char *crc_out);
void process_gps_output(struct gps_i2c_data *data);

int get_gps_data(double *latitude, double *longitude);
int get_gnrmc_data(struct GNRMC_GPS_data *data_out);
int get_gngga_data(struct GNGGA_GPS_data *data_out);
int GPS_thread();

#endif 