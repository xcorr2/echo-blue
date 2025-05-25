/*
 * M5 Core 2 Display Example
 * 
 * This example initializes the ILI9342C display and draws a test pattern.
 * Note: The M5 Core 2 uses AXP192 PMIC for power management. You may need
 * to initialize the AXP192 first to power on the display (axp_ld02) and
 * backlight (axp_dc3).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

/* RGB565 color definitions */
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define WHITE   0xFFFF
#define BLACK   0x0000
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F

static const struct device *display_dev;
static struct display_capabilities display_caps;

/* Buffer for drawing patterns */
static uint16_t display_buffer[320 * 240];

void draw_test_pattern(void)
{
    int width = display_caps.x_resolution;
    int height = display_caps.y_resolution;
    
    LOG_INF("Drawing test pattern on %dx%d display", width, height);
    
    /* Create a colorful test pattern */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t color;
            
            /* Create vertical color bars */
            if (x < width / 8) {
                color = RED;
            } else if (x < width * 2 / 8) {
                color = GREEN;
            } else if (x < width * 3 / 8) {
                color = BLUE;
            } else if (x < width * 4 / 8) {
                color = YELLOW;
            } else if (x < width * 5 / 8) {
                color = CYAN;
            } else if (x < width * 6 / 8) {
                color = MAGENTA;
            } else if (x < width * 7 / 8) {
                color = WHITE;
            } else {
                color = BLACK;
            }
            
            /* Add horizontal gradient effect */
            if (y > height / 2) {
                /* Dim the lower half */
                color = (color >> 1) & 0x7BEF; /* Divide RGB components by 2 */
            }
            
            display_buffer[y * width + x] = color;
        }
    }
    
    /* Write the buffer to display */
    struct display_buffer_descriptor desc = {
        .buf_size = width * height * 2, /* 2 bytes per pixel for RGB565 */
        .width = width,
        .height = height,
        .pitch = width,
    };
    
    int ret = display_write(display_dev, 0, 0, &desc, display_buffer);
    if (ret != 0) {
        LOG_ERR("Failed to write to display: %d", ret);
    } else {
        LOG_INF("Test pattern drawn successfully");
    }
}

void draw_simple_rectangles(void)
{
    int width = display_caps.x_resolution;
    int height = display_caps.y_resolution;
    
    LOG_INF("Drawing simple rectangles");
    
    /* Clear display first */
    memset(display_buffer, 0, sizeof(display_buffer));
    
    /* Draw colored rectangles */
    uint16_t colors[] = {RED, GREEN, BLUE, YELLOW};
    int rect_width = width / 2;
    int rect_height = height / 2;
    
    for (int rect = 0; rect < 4; rect++) {
        int start_x = (rect % 2) * rect_width;
        int start_y = (rect / 2) * rect_height;
        uint16_t color = colors[rect];
        
        for (int y = start_y; y < start_y + rect_height; y++) {
            for (int x = start_x; x < start_x + rect_width; x++) {
                if (x < width && y < height) {
                    display_buffer[y * width + x] = color;
                }
            }
        }
    }
    
    /* Write to display */
    struct display_buffer_descriptor desc = {
        .buf_size = width * height * 2,
        .width = width,
        .height = height,
        .pitch = width,
    };
    
    int ret = display_write(display_dev, 0, 0, &desc, display_buffer);
    if (ret != 0) {
        LOG_ERR("Failed to write rectangles to display: %d", ret);
    } else {
        LOG_INF("Rectangles drawn successfully");
    }
}

int main(void)
{
    LOG_INF("M5 Core 2 Display Example Starting");
    
    /* Get display device */
    display_dev = DEVICE_DT_GET(DISPLAY_NODE);
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -1;
    }
    
    LOG_INF("Display device ready");
    
    /* Get display capabilities */
    display_get_capabilities(display_dev, &display_caps);
    
    LOG_INF("Display capabilities:");
    LOG_INF("  Resolution: %dx%d", display_caps.x_resolution, display_caps.y_resolution);
    LOG_INF("  Pixel format: %d", display_caps.supported_pixel_formats);
    LOG_INF("  Screen info: %d", display_caps.screen_info);
    
    /* Turn off blanking (turn on display) */
    int ret = display_blanking_off(display_dev);
    if (ret != 0) {
        LOG_WRN("Could not turn off blanking: %d", ret);
    }
    
    /* 
     * NOTE: For M5 Core 2, you may need to initialize AXP192 PMIC here
     * to power on the display (axp_ld02) and backlight (axp_dc3).
     * This would require I2C communication with the AXP192.
     */
    
    LOG_INF("Starting display pattern demo");
    
    while (1) {
        /* Draw test pattern */
        draw_test_pattern();
        k_sleep(K_SECONDS(3));
        
        /* Draw simple rectangles */
        draw_simple_rectangles();
        k_sleep(K_SECONDS(3));
    }
    
    return 0;
}