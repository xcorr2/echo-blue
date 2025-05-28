#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/input/input.h>

#include "display.h"
#include "haptics.h"
#include "ui.h"

uint8_t display_update = 1;
uint8_t pressed = 0;

uint8_t station_index = DEFAULT_STATION_INDEX;
uint8_t record_timer_minutes = 1; // 0-9
uint8_t record_timer_seconds = 0; // 00-59

/**
 * 0 - Nothing
 * 1 - Valid Button Press
 * 2 - Invalid Button Press 
 * 3 - Manual Record Trigger
 * 4 - Emergency Record Trigger
 */
uint8_t haptic_response_case = 0;
uint8_t emergency_display_flag = 0;
uint8_t display_on_flag = 1;
uint8_t recording_start = 0;

// Shared variables for emergency sequence coordination
uint8_t emergency_cycle = 0;        // Current cycle state (0 or 1)
uint8_t emergency_count = 0;        // Current count (0-10)
uint8_t emergency_active = 0;       // Flag indicating emergency sequence is running
uint8_t previous_emergency_count = 0;

int main() {

    /* Setup touchscreen. */
    const struct device *touch_dev;
    touch_dev = DEVICE_DT_GET(DT_NODELABEL(ft5336_touch));
    if (!device_is_ready(touch_dev)) {
        printk("Touchscreen Setup Failed.\n");
        return -1;
    }

    display_init();
    haptics_init();
}

static void touch_input_callback(struct input_event *evt, void *user_data) {
    
    static int current_x = 0;
    static int current_y = 0;

    switch (evt->code) {
    case INPUT_ABS_X:
        current_y = evt->value;
        break;
    case INPUT_ABS_Y:
        current_x = evt->value;
        break;
    case INPUT_BTN_TOUCH:
        if (evt->value == 1 && !pressed) {

            printk("Touch PRESSED at (%d, %d)\n", current_x, current_y);
            pressed = 1;

            // Decrement station index:
            if (current_x > 105 && current_x < 155 && current_y > 18 && current_y < 80) {
                if (station_index > 0) {
                    station_index--;
                    haptic_response_case = VALID_BUTTON_PRESS;
                } else {
                    haptic_response_case = INVALID_BUTTON_PRESS;
                }

            // Increment station index:
            } else if (current_x > 250 && current_x < 315 && current_y > 18 && current_y < 80) {
                if (station_index < NUM_STATIONS - 1) {
                    station_index++;
                    haptic_response_case = VALID_BUTTON_PRESS;
                } else {
                    haptic_response_case = INVALID_BUTTON_PRESS;
                }

            // Decrement record timer:
            } else if (current_x > 105 && current_x < 165 && current_y > 95 && current_y < 155) {
                if (record_timer_seconds == 10 && record_timer_minutes == 0) {
                    haptic_response_case = INVALID_BUTTON_PRESS; // Min value, do nothing.
                } else {
                    haptic_response_case = VALID_BUTTON_PRESS;
                    if (record_timer_seconds >= 10) {
                        record_timer_seconds -= 10;
                    } else {
                        record_timer_minutes--;
                        record_timer_seconds += 50;
                    }
                }

            // Increment record timer:
            } else if (current_x > 250 && current_x < 315 && current_y > 95 && current_y < 155) {
                if (record_timer_minutes == 9 && record_timer_seconds >= 50) {
                    haptic_response_case = INVALID_BUTTON_PRESS; // Max value, do nothing.
                } else {
                    haptic_response_case = VALID_BUTTON_PRESS;
                    record_timer_seconds += 10;
                    if (record_timer_seconds >= 60) {
                        record_timer_minutes++;
                        record_timer_seconds = 0;
                    }
                }

            // Emergency button:
            } else if (current_x > 118 && current_x < 315 && current_y > 172 && current_y < 235) {
                emergency_display_flag = 1;
                recording_start = 1;
                haptic_response_case = EMERGENCY_RECORD_TRIGGER;
            }

        } else if (evt->value == 0 && pressed) {
            pressed = 0;
        }
        break;
    }
}

static void draw_display_background() {
    /* Header. */
    draw_rectangle(0, 0, 320, 12, BORDER_PURPLE);
    draw_text_8(8, 3, "CSSE4011        EMERGENCY ALERT SYSTEM", WHITE, BORDER_PURPLE);

    /* Section dividers. */
    for (int i = 0; i < 40; i++) {
        draw_rectangle(1+i*8, 86, 6, 3, BORDER_PURPLE);
        draw_rectangle(1+i*8, 163, 6, 3, BORDER_PURPLE);
    }

    draw_text_16(3, 30, "CENTER", WHITE, BLACK);
    draw_text_16(3, 50, "FREQ.", WHITE, BLACK);
    draw_text_16(185, 57, "MHz", WHITE, BLACK);

    draw_text_16(3, 107, "RECORD", WHITE, BLACK);
    draw_text_16(3, 127, "TIME", WHITE, BLACK);
    draw_text_16(178, 134, "mins", WHITE, BLACK);

    /* +/- Buttons. */
    draw_rectangle(111, 24, 50, 50, WHITE);
    draw_rectangle(113, 26, 46, 46, BORDER_PURPLE);
    draw_rectangle(258, 24, 50, 50, WHITE);
    draw_rectangle(260, 26, 46, 46, BORDER_PURPLE);

    draw_rectangle(111, 101, 50, 50, WHITE);
    draw_rectangle(113, 103, 46, 46, BORDER_PURPLE);
    draw_rectangle(258, 101, 50, 50, WHITE);
    draw_rectangle(260, 103, 46, 46, BORDER_PURPLE);

    /* Plus and minus symbols. */
    draw_rectangle(127, 47, 18, 3, WHITE);  
    draw_rectangle(274, 47, 18, 3, WHITE);  
    draw_rectangle(282, 40, 3, 18, WHITE);  
    draw_rectangle(127, 124, 18, 3, WHITE); 
    draw_rectangle(274, 124, 18, 3, WHITE); 
    draw_rectangle(282, 117, 3, 18, WHITE); 

    /* Emergency buttons. */
    draw_rectangle(124, 178, 184, 50, WHITE);
    draw_rectangle(126, 180, 180, 46, YELLOW);
    for (int i = 0; i < 6; i++) {
        draw_rectangle(139 + (i * 28), 180, 14, 46, BLACK);
    }
    draw_rectangle(126, 195, 180, 16, YELLOW);
    draw_text_16(144, 195, "EMERGENCY", BLACK, YELLOW);

}

static void draw_frequency(uint8_t station_index) {
    if (station_index > NUM_STATIONS - 1) {
        station_index = NUM_STATIONS - 1;
    }
    if (station_index <= 0) {
        station_index = 0;
    }
    draw_text_16(170, 32, stations[station_index], WHITE, BLACK);
}

static void draw_record_duration(uint8_t record_timer_minutes, 
                                 uint8_t record_timer_seconds) {
    if (record_timer_minutes > 9) {
        record_timer_minutes = 9;
    }
    if (record_timer_seconds > 59) {
        record_timer_seconds = 59;
    }
    
    static char time_str[5];  
    snprintf(time_str, sizeof(time_str), "%d:%02d", 
             record_timer_minutes, record_timer_seconds);
    draw_text_16(178, 109, time_str, WHITE, BLACK);
}

uint32_t get_record_duration_ms(uint8_t record_timer_minutes, 
                                uint8_t record_timer_seconds) {
    return (record_timer_minutes * 60 + record_timer_seconds) * 1000;
}

static void update_recording_status(uint8_t recording_status) {
    if (recording_status) {
        draw_text_8(167, 230, "RECORDING ON!", WHITE, BLACK);
    } else {
        draw_text_8(164, 230, "RECORDING OFF!", WHITE, BLACK);
    }
}

void haptics_thread() {
    // Wait for setup to finish.
    k_sleep(K_MSEC(1000));

    while(1) {
        k_sleep(K_MSEC(10));
        if (haptic_response_case > 0) {
            //printk("%d\n", haptic_response_case);
            switch(haptic_response_case) {
                case VALID_BUTTON_PRESS:
                    haptics_start_strength(99);
                    k_sleep(K_MSEC(120));
                    haptics_stop();
                    break;

                case INVALID_BUTTON_PRESS:
                    haptics_start_strength(99);
                    k_sleep(K_MSEC(100));
                    haptics_stop();
                    k_sleep(K_MSEC(150));
                    haptics_start_strength(99);
                    k_sleep(K_MSEC(100));
                    haptics_stop();
                    break;

                case MANUAL_RECORD_TRIGGER:
                    break;

                case EMERGENCY_RECORD_TRIGGER:

                    emergency_cycle = 0;
                    emergency_count = 0;
                    emergency_active = 1;
                    display_update = 0;
                    previous_emergency_count = emergency_count; 

                    while (emergency_count < 5) {
                        haptics_start();
                        
                        // Ramp up haptic strength
                        for (int strength = 1; strength <= 100; strength += 1) {
                            haptics_set_strength(strength);
                            k_sleep(K_MSEC(4));
                        }
                        // Ramp down haptic strength
                        for (int strength = 100; strength >= 1; strength -= 1) {
                            haptics_set_strength(strength);
                            k_sleep(K_MSEC(4));
                        }
                        haptics_stop();
                        k_sleep(K_MSEC(100));
                        
                        // Update cycle and count for display thread
                        emergency_cycle = !emergency_cycle;
                        emergency_count++;
                    }
                    
                    // Emergency sequence finished
                    emergency_active = 0;
                    emergency_display_flag = 0;
                    display_update = 1;
                    break;

                default:
                    break;
            }
            haptic_response_case = 0;
        }
    }
}

void display_thread() {
    // Wait for setup to finish.
    k_sleep(K_MSEC(1000));

    static uint32_t recording_start_time = 0;
    static uint32_t recording_duration_ms = 0;

    while(1) {

        if (recording_start) {
            if (recording_start_time == 0) {
                recording_start_time = k_uptime_get_32();
                recording_duration_ms = get_record_duration_ms(record_timer_minutes, record_timer_seconds);
                printk("Recording started for %d ms\n", recording_duration_ms);
            } else {
                uint32_t current_time = k_uptime_get_32();
                if ((current_time - recording_start_time) >= recording_duration_ms) {
                    printk("Recording finished\n");
                    recording_start = 0;
                    recording_start_time = 0; 
                }
            }
        } else {
            recording_start_time = 0;
        }

        // Handle emergency display - show continuously while active
        if (emergency_display_flag && emergency_active) {
            trig_emergency_warning(emergency_cycle);
        } else if (display_update) {
            // Only show normal display when not in emergency mode
            clear_buffer();
            draw_display_background();
            draw_frequency(station_index);
            draw_record_duration(record_timer_minutes, record_timer_seconds);
            update_recording_status(recording_start);
            send_buffer();
        }

        k_sleep(K_MSEC(DISPLAY_UPDATE_INTERVAL_MS));
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(ft5336_touch)), 
                        touch_input_callback, NULL);

K_THREAD_DEFINE(haptics_thread_id, HAPTICS_THREAD_STACK_SIZE,
    haptics_thread, NULL, NULL, NULL,
    HAPTICS_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(display_thread_id, DISPLAY_THREAD_STACK_SIZE,
    display_thread, NULL, NULL, NULL,
    DISPLAY_THREAD_PRIORITY, 0, 0);