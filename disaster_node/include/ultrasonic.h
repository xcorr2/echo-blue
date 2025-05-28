#ifndef ULTRASONIC_H
#define ULTRASONIC_H

struct us_values {
    void *fifo_reserved;
    double us_values[4];
};

void ultrasonic();

#endif