#ifndef DISPLAY_H
#define DISPLAY_H

/* RGB565 color definitions */
#define RED     0xF800
#define GREEN   0x001F
#define BLUE    0x07E0
#define WHITE   0xFFFF
#define BLACK   0x0000
#define YELLOW  0x80FE//0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F

#define BORDER_PURPLE 0xB730

#define DISPLAY_HEIGHT 240
#define DISPLAY_WIDTH 320

//#define dynamic_display_buffer

#ifdef dynamic_display_buffer 
extern uint16_t *display_buffer;
#endif

struct font_info {
    const unsigned char *data;
    int char_width;
    int char_height;
    int first_char;
    int char_count;
};

int display_init(void);
int trig_emergency_warning(int state);

void draw_num_32(int x, int y, const char *text, 
                 uint16_t color, uint16_t bg_color);

void draw_text_16(int x, int y, const char *text, 
                  uint16_t color, uint16_t bg_color);

void draw_text_8(int x, int y, const char *text, 
                  uint16_t color, uint16_t bg_color);

void draw_rectangle(int x, int y, int width, 
                 int height, uint16_t color);

void fill_buffer(uint16_t color);
void clear_buffer();
void send_buffer();


#endif 