#ifndef HAPTICS_H_
#define HAPTICS_H_

/**
 * @brief Initialize the haptic feedback system
 * 
 * This function must be called before using haptics_start() or haptics_stop().
 * It initializes the vibration motor regulator.
 */
int haptics_init(void);

/**
 * @brief Start haptic feedback (vibration)
 * 
 * Enables the vibration motor at default strength. Can be called from any thread.
 */
void haptics_start(void);

/**
 * @brief Start haptic feedback with specified strength
 * 
 * Enables the vibration motor at specified strength level.
 * 
 * @param strength_percent Vibration strength as percentage (0-100)
 *                        0 = off, 100 = maximum strength
 */
void haptics_start_strength(uint8_t strength_percent);

/**
 * @brief Change vibration strength while motor is running
 * 
 * Adjusts the vibration strength without stopping the motor.
 * Motor must already be running (started with haptics_start* functions).
 * 
 * @param strength_percent New vibration strength as percentage (0-100)
 *                        0 = stop motor, 100 = maximum strength
 */
void haptics_set_strength(uint8_t strength_percent);

/**
 * @brief Stop haptic feedback (vibration)
 * 
 * Disables the vibration motor. Can be called from any thread.
 */
void haptics_stop(void);

#endif /* HAPTICS_H_ */