/**
 * @file sparkfun_i2c_gps.c
 * @brief Implementation for the SparkFun XA1110 GPS I2C module driver
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "gps.h"

#define GPS_READ_CHUNK_SIZE 32  

static char gps_nmea_buffer[512];
static int gps_nmea_pos = 0;

K_MUTEX_DEFINE(GNGGA_data_mutex);
struct GNGGA_GPS_data GNGGA_data;

K_MUTEX_DEFINE(GNRMC_data_mutex);
struct GNRMC_GPS_data GNRMC_data;

/* Initialize the GPS module */
#define I2C0_NODE DT_NODELABEL(i2c1)

int gps_init(struct gps_i2c_data *data)
{

    uint8_t dummy = 0;
    uint8_t response = 0;
    int ret;

    printk("GPS init: Starting...\n");
    printk("GPS init: Checking arguments...\n");

    if (data == NULL) {
        printk("GPS init: Invalid arguments\n");
        return -EINVAL;
    }

    printk("GPS init: Initializing data structure...\n");

    data->head = 0;
    data->tail = 0;
    data->debug_enabled = true;

    printk("GPS init: About to get I2C device: %s\n", DT_NODE_PATH(I2C0_NODE));
    
    data->i2c_dev = DEVICE_DT_GET(I2C0_NODE);
    printk("GPS init: After device_get_binding()\n");
    
    if (!device_is_ready(data->i2c_dev)) {
        printk("I2C: Device not ready: %s\n", DT_NODE_PATH(I2C0_NODE));
        return GPS_ERR_I2C_INIT;
    }
    
    printk("I2C device is ready\n");

    printk("Configuring I2C...\n");
    printk("Sending test byte to GPS at address 0x%02x...\n", MT333X_I2C_ADDR);

    struct i2c_msg write_msg;
    write_msg.buf = &dummy;
    write_msg.len = 1;
    write_msg.flags = I2C_MSG_WRITE;
    
    ret = i2c_transfer(data->i2c_dev, &write_msg, 1, MT333X_I2C_ADDR);
    if (ret != 0) {
        printk("I2C: Error writing to device: %d\n", ret);
        return GPS_ERR_I2C_INIT;
    }
    
    printk("Test byte sent successfully\n");
    printk("Reading response from GPS...\n");
    
    struct i2c_msg read_msg;
    read_msg.buf = &response;
    read_msg.len = 1;
    read_msg.flags = I2C_MSG_READ;
    
    ret = i2c_transfer(data->i2c_dev, &read_msg, 1, MT333X_I2C_ADDR);
    if (ret != 0) {
        printk("I2C: Error reading from device: %d\n", ret);
        return GPS_ERR_NO_RESPONSE;
    }
    
    printk("Response received: 0x%02x\n", response);

    return GPS_SUCCESS;
}

/* Check for and buffer new data from GPS */
int gps_check(struct gps_i2c_data *data)
{
    uint8_t buffer[GPS_READ_CHUNK_SIZE];
    int ret, i;

    if (data == NULL || data->i2c_dev == NULL) {
        return -EINVAL;
    }

    /* Read data in chunks (Zephyr I2C driver limitation) */
    for (i = 0; i < (GPS_MAX_PACKET_SIZE / GPS_READ_CHUNK_SIZE); i++) {
        ret = i2c_read(data->i2c_dev, buffer, GPS_READ_CHUNK_SIZE, MT333X_I2C_ADDR);
        if (ret != 0) {
            if (data->debug_enabled) {
                printk("I2C: Error reading from device: %d\n", ret);
            }
            return ret;
        }

        /* Process each byte */
        for (int j = 0; j < GPS_READ_CHUNK_SIZE; j++) {
            uint8_t incoming = buffer[j];
            
            if (incoming != 0x0A) {
                /* Store byte in circular buffer */
                data->gps_buffer[data->head] = incoming;
                data->head = (data->head + 1) % GPS_MAX_PACKET_SIZE;
                
                /* Check for buffer overrun */
                if (data->head == data->tail) {
                    if (data->debug_enabled) {
                        printk("GPS buffer overrun\n");
                    }
                    /* Increment tail to prevent overwriting old data */
                    data->tail = (data->tail + 1) % GPS_MAX_PACKET_SIZE;
                }
            }
        }
    }

    return GPS_SUCCESS;
}

/* Get number of bytes available to read */
uint8_t gps_available(struct gps_i2c_data *data)
{
    if (data == NULL) {
        return 0;
    }

    /* If buffer is empty, check GPS for new data */
    if (data->head == data->tail) {
        gps_check(data);
    }

    /* Calculate available bytes */
    if (data->head >= data->tail) {
        return data->head - data->tail;
    } else {
        return GPS_MAX_PACKET_SIZE - data->tail + data->head;
    }
}

/* Read one byte from buffer */
uint8_t gps_read(struct gps_i2c_data *data)
{
    uint8_t byte;

    if (data == NULL) {
        return 0;
    }

    if (data->tail == data->head) {
        return 0;  
    }

    byte = data->gps_buffer[data->tail];
    data->tail = (data->tail + 1) % GPS_MAX_PACKET_SIZE;

    return byte;
}

/* Send MTK packet to GPS */
int gps_send_mtk_packet(struct gps_i2c_data *data, const char *command, size_t len)
{
    int ret;
    const uint8_t chunk_size = 32;
    uint8_t i, chunks;
    
    if (data == NULL || command == NULL || len == 0 || len > 255) {
        return -EINVAL;
    }
    
    /* Calculate number of chunks to send */
    chunks = (len + chunk_size - 1) / chunk_size;
    
    /* Send in chunks */
    for (i = 0; i < chunks; i++) {
        size_t offset = i * chunk_size;
        size_t bytes_to_send = MIN(chunk_size, len - offset);
        
        ret = i2c_write(data->i2c_dev, (const uint8_t *)&command[offset], 
                     bytes_to_send, MT333X_I2C_ADDR);
        if (ret != 0) {
            if (data->debug_enabled) {
                printk("I2C: Error writing to device: %d\n", ret);
            }
            return ret;
        }
        
        /* GPS module needs time to process each chunk */
        k_msleep(10);
    }
    
    return GPS_SUCCESS;
}

/* Calculate CRC for MTK packets */
int gps_calc_crc(const char *sentence, char *crc_out)
{
    uint8_t crc = 0;

    size_t i, len;
    
    if (sentence == NULL || crc_out == NULL) {
        return -EINVAL;
    }
    
    len = strlen(sentence);
    if (len < 2) {
        return -EINVAL;
    }
    
    /* Skip '$' at beginning and '*' at end if present */
    for (i = (sentence[0] == '$' ? 1 : 0); 
         i < len && sentence[i] != '*'; i++) {
        crc ^= sentence[i];
    }
    
    /* Format CRC as two-digit hex */
    sprintf(crc_out, "%02X", crc);
    
    return GPS_SUCCESS;
}

/* Create an MTK packet for GPS configuration. */
int gps_create_mtk_packet(uint16_t packet_type, const char *data_field, 
                         char *buffer, size_t buffer_size)
{
    int written = 0;
    char crc[3];
    
    if (buffer == NULL || buffer_size < 16) {
        return -EINVAL;
    }
    
    written = snprintf(buffer, buffer_size, "$PMTK%03d", packet_type);
    if (written < 0 || written >= buffer_size) {
        return -ENOMEM;
    }

    if (data_field != NULL && strlen(data_field) > 0) {
        int data_len = strlen(data_field);
        if (written + data_len >= buffer_size) {
            return -ENOMEM;
        }
        memcpy(buffer + written, data_field, data_len);
        written += data_len;
    }
    
    /* Add asterisk for CRC. */
    if (written + 5 >= buffer_size) {  /* Need space for "*XX\r\n\0" */
        return -ENOMEM;
    }
    buffer[written++] = '*';
    buffer[written] = '\0';
    
    /* Calculate and add CRC. */
    gps_calc_crc(buffer, crc);
    buffer[written++] = crc[0];
    buffer[written++] = crc[1];
    
    /* Termination bytes. */
    buffer[written++] = '\r';
    buffer[written++] = '\n';
    buffer[written] = '\0';
    
    return written;
}

/**
 * @brief Process GPGSV message (GPS satellites in view).
 */
static void process_gpgsv(const char *sentence) {
    //printk("-- GPGSV Packet Recieved -- \n");
}

/**
 * @brief Process GPGSA message (GPS DOP and active satellites).
 */
static void process_gpgsa(const char *sentence) {
    //printk("-- GPGSA Packet Recieved -- \n");
}

/**
 * @brief Process GLGSV message (GLONASS satellites in view).
 */
static void process_glgsv(const char *sentence) {
    //printk("-- GLGSV Packet Recieved -- \n");
}

/**
 * @brief Process GLGSA message (GLONASS DOP and active satellites).
 */
static void process_glgsa(const char *sentence) {
    //printk("-- GLGSA Packet Recieved -- \n");
}

/**
 * @brief Process GNVTG message (course over ground and ground speed).
 */
static void process_gnvtg(const char *sentence) {
    //printk("-- GNVTG Packet Recieved -- \n");
}


/**
 * @brief Process GNRMC message (recommended minimum navigation information).
 */
static int process_gnrmc(const char *sentence) {

    //printk("-- GNRMC Packet Recieved -- \n");

    k_mutex_lock(&GNRMC_data_mutex, K_FOREVER);

    // Make a mutable copy of the sentence
    char buffer[128];
    strncpy(buffer, sentence, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination

    char *token;
    int field = 0;
    int parsed_fields = 0;

    token = strtok(buffer, ",");
    while (token != NULL) {
        switch (field) {
            case 1:  // Time
                GNRMC_data.timestamp = atof(token);
                parsed_fields++;
                break;
            case 3:  // Latitude
                GNRMC_data.latitude = atof(token);
                parsed_fields++;
                break;
            case 4:  // N/S indicator
                GNRMC_data.lat_direction = token[0];
                parsed_fields++;
                break;
            case 5:  // Longitude
                GNRMC_data.longitude = atof(token);
                parsed_fields++;
                break;
            case 6:  // E/W indicator
                GNRMC_data.lon_direction = token[0];
                parsed_fields++;
                break;
            case 7:  // Speed
                GNRMC_data.speed = atof(token);
                parsed_fields++;
                break;
            case 8:  // Course
                GNRMC_data.course = atof(token);
                parsed_fields++;
                break;
        }

        token = strtok(NULL, ",");
        field++;
        printf("%s\n", sentence);
    }

    k_mutex_unlock(&GNRMC_data_mutex);

    if (parsed_fields == 7) {
        //printk("Successfully updated GNRMC data.\n");
        GNRMC_data.lock = 1;
        return 1;
    }

    //printk("Failed to retrieve GNRMC data, got %d fields.\n", parsed_fields);
    GNRMC_data.lock = 0;
    return 0;
}


/**
 * @brief Process GNGGA message (position fix data).
 */
static int process_gngga(const char *sentence) {

    printk("-- GNGGA Packet Recieved -- \n");

    k_mutex_lock(&GNGGA_data_mutex, K_FOREVER);
    printf("test2\n");

    for (int i = 0; gps_nmea_buffer[i] != '\0'; i++) {
        printk("%c", gps_nmea_buffer[i]);
    }
    printk("\n");
    memset(&GNGGA_data, 0, sizeof(GNGGA_data));

/*
        GNGGA_data.timestamp = 0.0;      // Time
        GNGGA_data.latitude = 0.0;        // Latitude
        GNGGA_data.lat_direction = 'N';   // N/S indicator
        GNGGA_data.longitude = 0.0;       // Longitude
        GNGGA_data.lon_direction = 'E';   // E/W indicator
        GNGGA_data.fix_quality=0;     // Fix quality
        GNGGA_data.satellites_used = 0;

    char* testsent[64];   
    
    int result = sscanf(gps_nmea_buffer, "%*[^,],%lf,%lf,%c,%lf,%c,%d,%d",
        &GNGGA_data.timestamp,       // Time
        &GNGGA_data.latitude,        // Latitude
        &GNGGA_data.lat_direction,   // N/S indicator
        &GNGGA_data.longitude,       // Longitude
        &GNGGA_data.lon_direction,   // E/W indicator
        &GNGGA_data.fix_quality,     // Fix quality
        &GNGGA_data.satellites_used  // Satellites used
    ); */

    double timestamp, latitude, longitude;
    char lat_dir, lon_dir;
    int fix_quality, satellites_used;
    
    timestamp = 0.0;
    latitude = 0.0;
    longitude = 0.0;
    lat_dir = 'a';
    lon_dir = 'b';
    fix_quality = 0;
    satellites_used = 0;
    
    //scanf(setence, "GNGGA,%lf,%lf,%c,%lf,%c,%d,%d", timestamp, latitude, longitude, )
    char* sentence1 = "GNGGA,001533.799,212.142,E,52.33,C,0,0,,,M,,M,,*55";
    //printf("%s\n", sentence);
    printf("test5\n");
    int result = sscanf(sentence1, "GNGGA,%lf,%lf,%c,%lf,%c,%d,%d",
        &timestamp, &latitude, &lat_dir, &longitude, &lon_dir, 
        &fix_quality, &satellites_used);
    

    printf("test3\n");
    if (result == 7) {
        k_mutex_lock(&GNGGA_data_mutex, K_FOREVER);
        GNGGA_data.timestamp = timestamp;
        GNGGA_data.latitude = latitude;
        GNGGA_data.lat_direction = lat_dir;
        GNGGA_data.longitude = longitude;
        GNGGA_data.lon_direction = lon_dir;
        GNGGA_data.fix_quality = fix_quality;
        GNGGA_data.satellites_used = satellites_used;
        GNGGA_data.lock = 1;
        k_mutex_unlock(&GNGGA_data_mutex);
        return 1;
    }

    /*
        printf("%f\n", &GNGGA_data.timestamp);       // Time
        printf("%f\n", &GNGGA_data.latitude);        // Latitude
        printf("%f\n", &GNGGA_data.lat_direction);   // N/S indicator
        printf("%f\n", &GNGGA_data.longitude);       // Longitude
        printf("%f\n", &GNGGA_data.lon_direction);   // E/W indicator
        printf("%f\n", &GNGGA_data.fix_quality);    // Fix quality
        printf("%f\n", &GNGGA_data.satellites_used);  // Satellites used
    */

    printf("test\n");
    k_mutex_unlock(&GNGGA_data_mutex);

    if (result == 7) {
        //printk("Successfully updated GNGGA data.\n");
        GNGGA_data.lock = 1;
        return 1;
    }
    //printk("Failed to retrived GNGGA data, got %d.\n", result);
    GNGGA_data.lock = 0;
    return 0;
}

/**
 * @brief Process a complete NMEA sentence
 * 
 * Parses the message type and dispatches to appropriate handler
 * 
 * @param sentence Complete NMEA sentence
 */
static void process_nmea_sentence(const char *sentence) {

    // Prepare to check the 5 letter identifier. 
    char msg_type[6] = {0};     
    strncpy(msg_type, sentence, 5);
    msg_type[5] = '\0'; 

    if (strcmp(msg_type, "GNGGA") == 0) {
        //process_gngga(sentence);
    } else if (strcmp(msg_type, "GNRMC") == 0) {
        process_gnrmc(sentence);
    } else if (strcmp(msg_type, "$GNVTG") == 0) {
        process_gnvtg(sentence);
    } else if (strcmp(msg_type, "GPGSV") == 0) {
        process_gpgsv(sentence);
    } else if (strcmp(msg_type, "GPGSA") == 0) {
        process_gpgsa(sentence);
    } else if (strcmp(msg_type, "GLGSV") == 0) {
        process_glgsv(sentence);
    } else if (strcmp(msg_type, "GLGSA") == 0) {
        process_glgsa(sentence);
    }
}

/**
 * -- GPS --
 * GPGSV - GPS satellites in view. 
 * $GPGSV,total_msgs,msg_num,sats_in_view,sat1_prn,sat1_elev,sat1_azimuth,sat1_snr,...,sat4_prn,sat4_elev,sat
 * 
 * GPGSA - GNSS DOP and Active Satellites
 * $GPGSA,mode,fix_type,sat1,sat2,...,sat12,pdop,hdop,vdop*checksum
 * 
 * -- GLONASS --
 * GLGSV - GLONASS satellites in view.
 * $GLGSV,total_msgs,msg_num,sats_in_view,sat1_prn,sat1_elev,sat1_azimuth,sat1_snr,...,sat4_prn,sat4_elev,sat
 * 
 * GPLSA - GNSS DOP and Active Satellites
 * $GLGSA,mode,fix_type,sat1,sat2,...,sat12,pdop,hdop,vdop*checksum
 * 
 * -- GN (Combined GNSS) -- 
 * GNRMC - Minimum Navigation Information.
 * $GNRMC,time,status,latitude,N/S,longitude,E/W,speed,course,date,mag_var,var_dir,mode*checksum
 * 
 * GNVTG - Course Over Ground and Ground Speed
 * $GNVTG,course_true,T,course_mag,M,speed_knots,N,speed_kmh,K,mode*checksum
 * 
 * GNGGA - Position System Fix Data
 * $GNGGA,time,latitude,N/S,longitude,E/W,fix,sats_used,hdop,altitude,M,geoid_height,M,dgps_age,dgps_station*checksu
 * 
 */

 void process_gps_output(struct gps_i2c_data *gps_data) {

    char byte;
    
    while (gps_available(gps_data)) {

        byte = gps_read(gps_data); 

        /* Add bytes to buffer. */
        if (byte != '$') {
            if (gps_nmea_pos < sizeof(gps_nmea_buffer) - 1) {
                gps_nmea_buffer[gps_nmea_pos++] = byte;
                gps_nmea_buffer[gps_nmea_pos] = '\0';
            }

        /* Sentence complete, handle processing. */
        } else if (byte == '$') {
            
            /*for (int i = 0; gps_nmea_buffer[i] != '\0'; i++) {
                printk("%c", gps_nmea_buffer[i]);
            }
            printk("\n");*/
            
            process_nmea_sentence(gps_nmea_buffer);
            memset(gps_nmea_buffer, 0, sizeof gps_nmea_buffer);
            gps_nmea_pos = 0;
        }
    }
}

double convert_nmea_to_decimal_degrees(double nmea_coord, char direction) {

    int degrees = (int)(nmea_coord / 100);
    double minutes = nmea_coord - (degrees * 100);
    double decimal_degrees = degrees + (minutes / 60.0);
    
    if (direction == 'S' || direction == 'W') {
        decimal_degrees = -decimal_degrees;
    }
    
    return decimal_degrees;
}

int get_gngga_data(struct GNGGA_GPS_data *data_out) {

    if (data_out == NULL) {
        return -EINVAL;
    }
    
    k_mutex_lock(&GNGGA_data_mutex, K_FOREVER);
    memcpy(data_out, &GNGGA_data, sizeof(GNGGA_data));
    k_mutex_unlock(&GNGGA_data_mutex);
    
    return 0;
}

int get_gnrmc_data(struct GNRMC_GPS_data *data_out) {

    if (data_out == NULL) {
        return -EINVAL;
    }
    
    k_mutex_lock(&GNRMC_data_mutex, K_FOREVER);
    memcpy(data_out, &GNRMC_data, sizeof(GNRMC_data));
    k_mutex_unlock(&GNRMC_data_mutex);
    
    return 0;
}

int get_gps_data(double *latitude, double *longitude) {

    int status = 0;
    double rmc_temp_lat, rmc_temp_long, gga_temp_lat, gga_temp_long;

    //k_mutex_lock(&GNRMC_data_mutex, K_FOREVER);
    //k_mutex_lock(&GNGGA_data_mutex, K_FOREVER);

    if (!GNGGA_data.lock && !GNRMC_data.lock) {
        *latitude = 0.0;
        *longitude = 0.0; 
        return status;
    }

    if (GNGGA_data.lock) {
        gga_temp_lat = convert_nmea_to_decimal_degrees(GNGGA_data.latitude, 
            GNGGA_data.lat_direction);
        gga_temp_long = convert_nmea_to_decimal_degrees(GNGGA_data.longitude, 
            GNGGA_data.lon_direction);
        status++;
    }

    if (GNRMC_data.lock) {
        rmc_temp_lat = convert_nmea_to_decimal_degrees(GNRMC_data.latitude, 
            GNRMC_data.lat_direction);
        rmc_temp_long = convert_nmea_to_decimal_degrees(GNRMC_data.longitude, 
            GNRMC_data.lon_direction);
        status++;
    }

    if (status == 2) {
        *latitude = (gga_temp_lat + rmc_temp_lat) / 2;
        *longitude = (gga_temp_long + rmc_temp_long) / 2; 
    } else if (GNRMC_data.lock) {
        *latitude = rmc_temp_lat;
        *longitude = rmc_temp_long;
    } else if (GNGGA_data.lock) {
        *latitude = gga_temp_lat;
        *longitude = gga_temp_long;
    }

    //k_mutex_unlock(&GNRMC_data_mutex);
    //k_mutex_unlock(&GNGGA_data_mutex);

    return status; // Number of sources. 
}

int GPS_thread() {

    int ret;
    char config_packet[64];
    static struct gps_i2c_data gps_data;

    /* Initialize GPS */
    ret = gps_init(&gps_data);
    if (ret != 0) {
        printk("GPS initialization failed: %d\n", ret);
        return 0;
    }

    printk("GPS module found!\n");

    /* Configure the GPS module - Enable PPS LED */
    ret = gps_create_mtk_packet(314, ",0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0", config_packet, sizeof(config_packet));
    if (ret > 0) {
        ret = gps_send_mtk_packet(&gps_data, config_packet, ret);
        if (ret != 0) {
            printk("Failed to send configuration packet: %d\n", ret);
        }
    }

    /* 5 second delay to allow for start-up. */
    k_msleep(5000);

    while (1) {
        process_gps_output(&gps_data);
        k_msleep(100);
    }

    return 0;

}