#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/regulator.h>
#include "haptics.h"

/* Get the vibration motor regulator device */
static const struct device *vib_regulator = DEVICE_DT_GET(DT_NODELABEL(vib_motor));

/* Track current state */
static bool haptics_enabled = false;

/* Voltage range for vibration motor (in microvolts) */
#define HAPTICS_MIN_VOLTAGE_UV  2000000  /* 2.0V - minimum for motor operation */
#define HAPTICS_MAX_VOLTAGE_UV  3000000  /* 3.0V - maximum safe voltage */
#define HAPTICS_DEFAULT_VOLTAGE_UV 2800000  /* 2.8V - default strength */

/* Helper function to calculate voltage from percentage */
static uint32_t strength_to_voltage(uint8_t strength_percent)
{
    if (strength_percent > 100) {
        strength_percent = 100;
    }
    
    uint32_t voltage_range = HAPTICS_MAX_VOLTAGE_UV - HAPTICS_MIN_VOLTAGE_UV;
    return HAPTICS_MIN_VOLTAGE_UV + (voltage_range * strength_percent) / 100;
}

int haptics_init(void)
{
    int ret;
    /* Check if the regulator device is ready */
    ret = device_is_ready(vib_regulator); 
    if (!ret) {
        printk("Vibration motor regulator device not ready\n");
        return ret;
    }
    
    /* Ensure vibration is initially off */
    regulator_disable(vib_regulator);
    haptics_enabled = false;
    return 0;
}

void haptics_start(void)
{
    /* Start with default strength */
    haptics_start_strength(100);
}

void haptics_start_strength(uint8_t strength_percent)
{
    if (!device_is_ready(vib_regulator)) {
        return;
    }
    
    if (strength_percent == 0) {
        haptics_stop();
        return;
    }
    
    /* Calculate and set the target voltage */
    uint32_t target_voltage = strength_to_voltage(strength_percent);
    regulator_set_voltage(vib_regulator, target_voltage, target_voltage);
    
    /* Enable the vibration motor regulator if not already enabled */
    if (!haptics_enabled) {
        regulator_enable(vib_regulator);
        haptics_enabled = true;
    }
}

void haptics_set_strength(uint8_t strength_percent)
{
    if (!device_is_ready(vib_regulator)) {
        return;
    }
    
    if (strength_percent == 0) {
        haptics_stop();
        return;
    }
    
    /* Only change voltage if motor is currently running */
    if (haptics_enabled) {
        uint32_t target_voltage = strength_to_voltage(strength_percent);
        regulator_set_voltage(vib_regulator, target_voltage, target_voltage);
    }
}

void haptics_stop(void)
{
    if (!device_is_ready(vib_regulator)) {
        return;
    }
    
    /* Disable the vibration motor regulator */
    regulator_disable(vib_regulator);
    haptics_enabled = false;
}