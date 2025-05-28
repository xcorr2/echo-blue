#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>

#define HAPTICS_THREAD_STACK_SIZE    4096
#define HAPTICS_THREAD_PRIORITY      5
#define DISPLAY_THREAD_STACK_SIZE    4096
#define DISPLAY_THREAD_PRIORITY      6

#define DEBOUNCE_INTERVAL_MS 20
#define DISPLAY_UPDATE_INTERVAL_MS 50

#define NOTHING 0
#define VALID_BUTTON_PRESS 1
#define INVALID_BUTTON_PRESS 2
#define MANUAL_RECORD_TRIGGER 3
#define EMERGENCY_RECORD_TRIGGER 4

#define DEFAULT_STATION_INDEX 2
#define NUM_STATIONS 10
static char* stations[NUM_STATIONS] = {
    " 93.3", " 96.5", " 97.3", "102.1",
    "103.7", "104.5", "105.3", "106.1",
    "106.9", "107.7"};

static void touch_input_callback(struct input_event *evt, void *user_data);
