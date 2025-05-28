#ifndef KALMAN_H
#define KALMAN_H

extern struct k_fifo kalman_us_fifo;
extern struct k_fifo kalman_rssi_fifo;
extern struct k_fifo kalman_rs_fifo;
extern struct k_fifo signal_fifo;

extern struct k_sem signal;

struct kalman_values {
        double x;
        double y;
        double vx;
        double vy;
        int sensor;
};

void create_filter();

#endif