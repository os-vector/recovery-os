#ifndef CORE_LCD_H_
#define CORE_LCD_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// maximum dimensions for any supported screen
#define MAX_LCD_WIDTH  184
#define MAX_LCD_HEIGHT 96

typedef struct LcdFrame_t {
    uint16_t data[MAX_LCD_WIDTH * MAX_LCD_HEIGHT];
} LcdFrame;

enum LcdColor {
    lcd_BLACK   = 0x0000,
    lcd_BLUE    = 0x001F,
    lcd_GREEN   = 0x07E0,
    lcd_CYAN    = 0x7FFF,
    lcd_GRAY    = 0x8430,
    lcd_RED     = 0xF800,
    lcd_MAGENTA = 0xF81F,
    lcd_YELLOW  = 0xFFE0,
    lcd_WHITE   = 0xFFFF,
};

int lcd_init(void);
void lcd_clear_screen(void);
void lcd_draw_frame(const LcdFrame* frame);
void lcd_draw_frame2(const uint16_t* frame, size_t size);
void lcd_set_brightness(int b); // 0..20
void lcd_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // CORE_LCD_H_

