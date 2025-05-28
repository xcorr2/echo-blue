/*
 * M5 Core 2 Display Example
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include <string.h>

#include "display.h"

//#define USE_BIG_NUMS

#ifdef USE_BIG_NUMS
#include "font32x48_nums.h"
#endif
#include "font16x16.h"
#include "font8x8.h"

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

/* Buffer for drawing patterns */
static uint16_t display_buffer[DISPLAY_HEIGHT * DISPLAY_WIDTH];

//uint16_t *display_buffer = NULL;
//size_t buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);

static const struct device *display_dev;
static struct display_capabilities display_caps;

struct display_buffer_descriptor desc = {
            .buf_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2,
            .width = DISPLAY_WIDTH,
            .height = DISPLAY_HEIGHT,
            .pitch = DISPLAY_WIDTH,
};



static void draw_test_pattern_1(void)
{
    int width = display_caps.x_resolution;
    int height = display_caps.y_resolution;
    
    printk("Drawing test pattern on %dx%d display\n", width, height);
    
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
        printk("Failed to write to display: %d\n", ret);
    } else {
        printk("Test pattern drawn successfully\n");
    }
}
/*
static void draw_test_pattern_2(void)
{
    int width = display_caps.x_resolution;
    int height = display_caps.y_resolution;
    
    printk("Drawing simple rectangles\n");
    
    // Clear display first.
    memset(display_buffer, 0, sizeof(display_buffer));
    
    // Draw colored rectangles.
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
    
    // Write to display. 
    struct display_buffer_descriptor desc = {
        .buf_size = width * height * 2,
        .width = width,
        .height = height,
        .pitch = width,
    };
    
    int ret = display_write(display_dev, 0, 0, &desc, display_buffer);
    if (ret != 0) {
        printk("Failed to write rectangles to display: %d\n", ret);
    } else {
        printk("Rectangles drawn successfully\n");
    }
}
*/
void fill_buffer(uint16_t color) {
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        display_buffer[i] = color;
    }
}

void draw_rectangle(int x, int y, int width, int height, uint16_t color) {
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int pixel_x = x + col;
            int pixel_y = y + row;
            
            if (pixel_x >= 0 && pixel_x < DISPLAY_WIDTH && 
                pixel_y >= 0 && pixel_y < DISPLAY_HEIGHT) {
                display_buffer[pixel_y * DISPLAY_WIDTH + pixel_x] = color;
            } else {
                printk("Bounding error in draw_rectangle!\n");
            }
        }
    }
}

static void draw_exclamation_mark(uint16_t *buffer, uint16_t color) {
    int center_x = DISPLAY_WIDTH / 2;
    int center_y = DISPLAY_HEIGHT / 2;
    
    int line_width = 12;    /* Width of vertical line */
    int line_height = 80;   /* Height of vertical line */
    int dot_size = 12;      /* Size of dot (square) */
    int gap = 15;           /* Gap between line and dot */
    
    int line_x = center_x - (line_width / 2);
    int line_y = center_y - (line_height / 2) - (gap / 2) - (dot_size / 2);
    
    int dot_x = center_x - (dot_size / 2);
    int dot_y = line_y + line_height + gap;
    
    draw_rectangle(line_x, line_y, line_width, line_height, color);
    draw_rectangle(dot_x, dot_y, dot_size, dot_size, color);

}

int trig_emergency_warning(int state) {

    //printk("Drawing Exclamation!\n");

    //memset(display_buffer, 0, sizeof(display_buffer));
    clear_buffer();

    int center_x = DISPLAY_WIDTH / 2;
    int center_y = DISPLAY_HEIGHT / 2;
    
    int line_width = 12;
    int line_height = 80;
    int dot_size = 12;
    int gap = 15;
    
    int line_x = center_x - (line_width / 2);
    int line_y = center_y - (line_height / 2) - (gap / 2) - (dot_size / 2);
    
    int dot_x = center_x - (dot_size / 2);
    int dot_y = line_y + line_height + gap;

    if (state) {
        fill_buffer(RED);
        draw_exclamation_mark(display_buffer, WHITE);
        
        // Position "RECORDING" between bottom of exclamation and bottom of screen
        int text_x = center_x - 72;  // 9 chars * 16 pixels / 2
        int text_y = (dot_y + dot_size + DISPLAY_HEIGHT) / 2 - 8;  // Centered vertically
        draw_text_16(text_x, text_y, "RECORDING", WHITE, RED);
        
    } else {
        fill_buffer(BLUE);
        draw_exclamation_mark(display_buffer, BLACK);
        
        // Position "RECORDING" between bottom of exclamation and bottom of screen
        int text_x = center_x - 72;  // 9 chars * 16 pixels / 2
        int text_y = (dot_y + dot_size + DISPLAY_HEIGHT) / 2 - 8;  // Centered vertically
        draw_text_16(text_x, text_y, "RECORDING", BLACK, BLUE);
    }

    int ret = display_write(display_dev, 0, 0, &desc, display_buffer);
    if (ret != 0) {
        printk("Failed to write rectangles to display: %d\n", ret);
    } else {
        //printk("Rectangles drawn successfully\n");
    }

    return 0;
}

void send_buffer() {
    display_write(display_dev, 0, 0, &desc, display_buffer);
}

void clear_buffer() {
    memset(display_buffer, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    //memset(display_buffer, 0, buffer_size);
}

#ifdef USE_BIG_NUMS
void draw_dot_32(uint16_t *buffer, int x, int y, uint16_t color, uint16_t bg_color) {

    const int dot_size = 8;
    const int dot_width = 16;  // Total advance width (narrower than numbers)
    const int dot_start_row = 35;  // Start drawing dot at row 35 (out of 48 total)
    const int dot_start_col = (dot_width - dot_size) / 2;  // Center horizontally
    
    // Draw background for the entire dot character area (16 pixels wide)
    for (int row = 0; row < 48; row++) {
        for (int col = 0; col < dot_width; col++) {
            int pixel_x = x + col;
            int pixel_y = y + row;
            
            // Bounds checking
            if (pixel_x >= DISPLAY_WIDTH || pixel_y >= DISPLAY_HEIGHT || 
                pixel_x < 0 || pixel_y < 0) {
                continue;
            }
            
            // Determine if this pixel is part of the dot
            bool is_dot = false;
            if (row >= dot_start_row && row < dot_start_row + dot_size &&
                col >= dot_start_col && col < dot_start_col + dot_size) {
                is_dot = true;
            }
            
            uint16_t pixel_color = is_dot ? color : bg_color;
            buffer[pixel_y * DISPLAY_WIDTH + pixel_x] = pixel_color;
        }
    }
}
#endif

void draw_string_variable(uint16_t *buffer, int x, int y, const char *text, 
                         const uint8_t *font, uint16_t color, uint16_t bg_color) {

    uint8_t font_width = font[2];
    uint8_t font_height = font[3];
    uint8_t first_char = font[4];
    uint8_t char_count = font[5];
    const uint8_t *font_data = &font[6];
    
    int current_x = x;
    
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        
        #ifdef USE_BIG_NUMS
        if (c == '.' && font == fixednums32x48) {
            draw_dot_32(buffer, current_x, y, color, bg_color);
            current_x += 16; 
            continue;
        }
        #endif
        
        if (c < first_char || c >= first_char + char_count) {
            current_x += font_width;
            continue;
        }
        
        int char_index = c - first_char;
        
        int bytes_per_char;
        if (font_width == 8) {
            bytes_per_char = font_height; // 1 byte per row for 8-pixel width
        } else if (font_width == 16) {
            bytes_per_char = font_height * 2; // 2 bytes per row for 16-pixel width
        } else if (font_width == 32) {
            bytes_per_char = font_height * 4; // 4 bytes per row for 32-pixel width
        } else {
            // Unsupported font width, skip
            current_x += font_width;
            continue;
        }
        
        const uint8_t *char_data = font_data + (char_index * bytes_per_char);
        
        for (int row = 0; row < font_height; row++) {
            for (int col = 0; col < font_width; col++) {
                int pixel_x = current_x + col;
                int pixel_y = y + row;

                if (pixel_x >= DISPLAY_WIDTH || pixel_y >= DISPLAY_HEIGHT || 
                    pixel_x < 0 || pixel_y < 0) {
                    continue;
                }
                
                bool pixel_set = false;
                
                if (font_width == 8) {
                    // 8-bit font: 1 byte per row
                    uint8_t row_data = char_data[row];
                    pixel_set = (row_data & (1 << col)) != 0;
                } else if (font_width == 16) {
                    // 16-bit font: 2 bytes per row, big-endian format
                    uint16_t row_data = (char_data[row * 2] << 8) | char_data[row * 2 + 1];
                    pixel_set = (row_data & (0x8000 >> col)) != 0;
                } else if (font_width == 32) {
                    // 32-bit font: 4 bytes per row, big-endian format
                    int byte_index = row * 4 + (col / 8);        // Which byte in this row
                    int bit_index = 7 - (col % 8);               // Which bit in that byte
                    uint8_t byte_data = char_data[byte_index];
                    pixel_set = (byte_data & (1 << bit_index)) != 0;
                }
                uint16_t pixel_color = pixel_set ? color : bg_color;
                buffer[pixel_y * DISPLAY_WIDTH + pixel_x] = pixel_color;
            }
        }
        current_x += font_width;
    }
}

void draw_text_8(int x, int y, const char *text, 
                  uint16_t color, uint16_t bg_color) {
    draw_string_variable(display_buffer, x, y, text, font8x8, color, bg_color);
}

void draw_text_16(int x, int y, const char *text, 
                  uint16_t color, uint16_t bg_color) {
    draw_string_variable(display_buffer, x, y, text, font16x16, color, bg_color);
}

#ifdef USE_BIG_NUMS
void draw_num_32(int x, int y, const char *text, 
                 uint16_t color, uint16_t bg_color) {

    for (int i = 0; text[i] != '\0'; i++) {
        if ((text[i] < '0' || text[i] > '9') && text[i] != '.') {
            return;
        }
    }
    
    draw_string_variable(display_buffer, x, y, text, fixednums32x48, color, bg_color);
}
#endif

int display_init(void) {
    printk("M5 Core 2 Display Example Starting\n");
    
    /* Get display device */
    display_dev = DEVICE_DT_GET(DISPLAY_NODE);
    if (!device_is_ready(display_dev)) {
        printk("Display device not ready\n");
        return -1;
    }
    
    printk("Display device ready\n");
    
    /* Get display capabilities */
    display_get_capabilities(display_dev, &display_caps);
    
    printk("  === Display Capabilities ===\n");
    printk("  Resolution: %dx%d\n", display_caps.x_resolution, display_caps.y_resolution);
    printk("  Pixel format: %d\n", display_caps.supported_pixel_formats);
    printk("  Screen info: %d\n", display_caps.screen_info);
    
    /*
    size_t buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    display_buffer = k_calloc(DISPLAY_WIDTH * DISPLAY_HEIGHT, sizeof(uint16_t));
    if (!display_buffer) {
        printk("Failed to allocate display buffer\n");
        return -ENOMEM;
    }
    */

    /* Turn off blanking (turn on display) */
    int ret = display_blanking_off(display_dev);
    if (ret != 0) {
        printk("Could not turn off blanking: %d", ret);
    }
    
    printk("Starting display pattern demo\n");

    draw_test_pattern_1();
    k_sleep(K_MSEC(10));
    clear_buffer();

    return 0;
}