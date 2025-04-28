#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <linux/spi/spidev.h>

#include "core/common.h"
#include "core/clock.h"
#include "core/gpio.h"
#include "core/anki_dev_unit.h"
#include "core/lcd.h"

// gpio pin assignments
#define GPIO_LCD_WRX    110
#define GPIO_LCD_RESET1 96
#define GPIO_LCD_RESET2 55

#define FALSE 0
#define TRUE  (!FALSE)

#define MSB(a) ((a) >> 8)
#define LSB(a) ((a) & 0xff)
#define ST_SCREEN_WIDTH  184
#define ST_SCREEN_HEIGHT 96

#define NV_SCREEN_WIDTH  160
#define NV_SCREEN_HEIGHT 80

// shifting values for column/row settings
#define RSHIFT 0x1C
#define XSHIFT 0x0
#define YSHIFT 0x18

static int MAX_TRANSFER = 0x1000;

static int lcd_frame_width;
static int lcd_frame_height;
static int lcd_pixel_count;
static int use_nv_screen = 0; // 1 = nv3022, 0 = st77890

// spi fd and gpio handles
static int spi_fd;
static GPIO DnC_PIN;
static GPIO RESET_PIN1;
static GPIO RESET_PIN2;

// init script structure
typedef struct {
    uint8_t cmd;
    uint8_t data_bytes;
    uint8_t data[128];
    uint32_t delay_ms;
} INIT_SCRIPT;

// st77890 init script (for 184x96 screen)
static const INIT_SCRIPT st77890_init_scr[] = {
    { 0x10, 1, { 0x00 }, 120 }, // sleep in
    { 0x2A, 4, { 0x00, RSHIFT, (ST_SCREEN_WIDTH + RSHIFT - 1) >> 8, (ST_SCREEN_WIDTH + RSHIFT - 1) & 0xFF }, 0 },
    { 0x2B, 4, { 0x00, 0x00, (ST_SCREEN_HEIGHT - 1) >> 8, (ST_SCREEN_HEIGHT - 1) & 0xFF }, 0 },
    { 0x36, 1, { 0x00 }, 0 },
    { 0x3A, 1, { 0x55 }, 0 },
    { 0xB0, 2, { 0x00, 0x08 }, 0 },
    { 0xB2, 5, { 0x0C, 0x0C, 0x00, 0x33, 0x33 }, 0 },
    { 0xB7, 1, { 0x72 }, 0 },
    { 0xBB, 1, { 0x3B }, 0 },
    { 0xC0, 1, { 0x2C }, 0 },
    { 0xC2, 1, { 0x01 }, 0 },
    { 0xC3, 1, { 0x14 }, 0 },
    { 0xC4, 1, { 0x20 }, 0 },
    { 0xC6, 1, { 0x0F }, 0 },
    { 0xD0, 2, { 0xA4, 0xA1 }, 0 },
    { 0xE0, 14, { 0xD0, 0x10, 0x16, 0x0A, 0x0A, 0x26, 0x3C, 0x53, 0x53, 0x18, 0x15, 0x12, 0x36, 0x3C }, 0 },
    { 0xE1, 14, { 0xD0, 0x11, 0x19, 0x0A, 0x09, 0x25, 0x3D, 0x35, 0x54, 0x17, 0x15, 0x12, 0x36, 0x3C }, 0 },
    { 0xE9, 3, { 0x05, 0x05, 0x01 }, 0 },
    { 0x21, 1, { 0x00 }, 0 },
    { 0 }
};

static const INIT_SCRIPT st77890_display_on_scr[] = {
    { 0x11, 1, { 0x00 }, 120 },
    { 0x29, 1, { 0x00 }, 120 },
    { 0 }
};

// nv3022 init script (for 160x80 screen)
static const INIT_SCRIPT nv3022_init_scr[] = {
    { 0x01, 0, { 0x00 }, 150 }, // software reset
    { 0x11, 0, { 0x00 }, 500 }, // sleep out
    { 0x20, 0, { 0x00 }, 0 },   // inversion off
    { 0x36, 1, { 0xA8 }, 0 },
    { 0x3A, 1, { 0x05 }, 0 },
    { 0xE0, 16, { 0x07, 0x0e, 0x08, 0x07, 0x10, 0x07, 0x02, 0x07,
                   0x09, 0x0f, 0x25, 0x36, 0x00, 0x08, 0x04, 0x10 }, 0 },
    { 0xE1, 16, { 0x0a, 0x0d, 0x08, 0x07, 0x0f, 0x07, 0x02, 0x07,
                   0x09, 0x0f, 0x25, 0x35, 0x00, 0x09, 0x04, 0x10 }, 0 },
    { 0xFC, 1, { (uint8_t)(128+64) }, 0 },
    { 0x13, 0, { 0x00 }, 100 },
    { 0x26, 1, { 0x02 }, 10 },
    { 0x29, 0, { 0x00 }, 10 },
    { 0x2A, 4, { MSB(XSHIFT), LSB(XSHIFT),
                   MSB(NV_SCREEN_WIDTH + XSHIFT - 1), LSB(NV_SCREEN_WIDTH + XSHIFT - 1) }, 0 },
    { 0x2B, 4, { MSB(YSHIFT), LSB(YSHIFT),
                   MSB(NV_SCREEN_HEIGHT + YSHIFT - 1), LSB(NV_SCREEN_HEIGHT + YSHIFT - 1) }, 0 },
    { 0 }
};

static const INIT_SCRIPT nv3022_display_on_scr[] = {
    { 0x11, 1, { 0x00 }, 120 },
    { 0x29, 1, { 0x00 }, 120 },
    { 0 }
};

// hw version detector
static uint32_t get_vector_hw_version() {
    int fd = open("/dev/mmcblk0p29", O_RDONLY);
    if (fd < 0) {
        error_exit(app_DEVICE_OPEN_ERROR, "can't open hw partition: %d\n", errno);
    }
    uint32_t emr_data[8] = {0};
    int res = read(fd, emr_data, sizeof(emr_data));
    if (res < 0) {
        error_exit(app_DEVICE_OPEN_ERROR, "failed to read hw partition: %d\n", errno);
    }
    close(fd);
    return emr_data[1];  // assume hw version is stored here
}

static int lcd_spi_init() {
    uint8_t mode = 0;
    int fd = open("/dev/spidev1.0", O_RDWR);
    if (fd < 0) {
        error_exit(app_DEVICE_OPEN_ERROR, "can't open lcd spi interface: %d\n", errno);
    }
    if (ioctl(fd, SPI_IOC_RD_MODE, &mode) < 0) {
        error_exit(app_IO_ERROR, "can't set spi mode: %d\n", errno);
    }
    return fd;
}

static void lcd_spi_transfer(int cmd, int bytes, const void* data) {
    const uint8_t* tx_buf = data;
    gpio_set_value(DnC_PIN, cmd ? gpio_LOW : gpio_HIGH);
    while (bytes > 0) {
        size_t count = bytes > MAX_TRANSFER ? MAX_TRANSFER : bytes;
        (void)write(spi_fd, tx_buf, count);
        bytes -= count;
        tx_buf += count;
    }
}

static void lcd_run_script(const INIT_SCRIPT* script) {
    for (int i = 0; script[i].cmd != 0; i++) {
        lcd_spi_transfer(TRUE, 1, &script[i].cmd);
        lcd_spi_transfer(FALSE, script[i].data_bytes, script[i].data);
        milliwait(script[i].delay_ms);
    }
}

static void lcd_device_init() {
    // run the proper init sequence based on detected hw
    if (use_nv_screen) {
        lcd_run_script(nv3022_init_scr);
        lcd_run_script(nv3022_display_on_scr);
    } else {
        lcd_run_script(st77890_init_scr);
        lcd_run_script(st77890_display_on_scr);
    }
    lcd_clear_screen();
}

// updated: swap pixel data for st77890 screens if needed
void lcd_draw_frame(const LcdFrame* frame) {
    static const uint8_t WRITE_RAM = 0x2C;
    if (use_nv_screen) {
        static uint16_t buffer[160 * 80];
        for (int i = 0; i < lcd_pixel_count; i++) {
            buffer[i] = __builtin_bswap16(frame->data[i]);
        }
        lcd_spi_transfer(TRUE, 1, &WRITE_RAM);
        lcd_spi_transfer(FALSE, lcd_pixel_count * sizeof(uint16_t), buffer);
    } else {
        lcd_spi_transfer(TRUE, 1, &WRITE_RAM);
        lcd_spi_transfer(FALSE, lcd_pixel_count * sizeof(uint16_t), frame->data);
    }
}

void lcd_draw_frame2(const uint16_t* frame, size_t size) {
    static const uint8_t WRITE_RAM = 0x2C;
    if (use_nv_screen) {
        static uint16_t buffer[160 * 80];
        for (int i = 0; i < lcd_pixel_count; i++) {
            buffer[i] = __builtin_bswap16(frame[i]);
        }
        lcd_spi_transfer(TRUE, 1, &WRITE_RAM);
        lcd_spi_transfer(FALSE, lcd_pixel_count * sizeof(uint16_t), buffer);
    } else {
        lcd_spi_transfer(TRUE, 1, &WRITE_RAM);
        lcd_spi_transfer(FALSE, lcd_pixel_count * sizeof(uint16_t), frame);
    }
}

void lcd_clear_screen(void) {
    LcdFrame frame;
    memset(frame.data, 0, sizeof(frame.data));
    lcd_draw_frame(&frame);
}

void lcd_set_brightness(int brightness) {
    brightness = (brightness > 20) ? 20 : brightness;
    brightness = (brightness < 0) ? 0 : brightness;
    const char* BACKLIGHT_DEVICES[] = {
        "/sys/class/leds/face-backlight-left/brightness",
        "/sys/class/leds/face-backlight-right/brightness"
    };
    for (int l = 0; l < 2; l++) {
        int fd = open(BACKLIGHT_DEVICES[l], O_WRONLY);
        if (fd >= 0) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02d\n", brightness);
            (void)write(fd, buf, strlen(buf));
            close(fd);
        }
    }
}

int lcd_init(void) {
    lcd_set_brightness(10);

    // auto-detect hw version
    uint32_t hw_ver = get_vector_hw_version();
    if (hw_ver > 7) {
        use_nv_screen = 1;
        lcd_frame_width  = NV_SCREEN_WIDTH;
        lcd_frame_height = NV_SCREEN_HEIGHT;
        lcd_pixel_count  = NV_SCREEN_WIDTH * NV_SCREEN_HEIGHT;
    } else {
        use_nv_screen = 0;
        lcd_frame_width  = ST_SCREEN_WIDTH;
        lcd_frame_height = ST_SCREEN_HEIGHT;
        lcd_pixel_count  = ST_SCREEN_WIDTH * ST_SCREEN_HEIGHT;
    }

    // setup gpio for DnC and resets
    DnC_PIN    = gpio_create(GPIO_LCD_WRX, gpio_DIR_OUTPUT, gpio_HIGH);
    RESET_PIN1 = gpio_create_open_drain_output(GPIO_LCD_RESET1, gpio_HIGH);
    RESET_PIN2 = gpio_create(GPIO_LCD_RESET2, gpio_DIR_OUTPUT, gpio_HIGH);
    usleep(200);

    spi_fd = lcd_spi_init();

    // hardware reset sequence
    microwait(50);
    gpio_set_value(RESET_PIN1, 0);
    gpio_set_value(RESET_PIN2, 0);
    usleep(50);
    gpio_set_value(RESET_PIN1, 1);
    gpio_set_value(RESET_PIN2, 1);
    usleep(250);

    lcd_device_init();
    return 0;
}

void lcd_shutdown(void) {
    if (spi_fd) {
        static const uint8_t SLEEP = 0x10;
        lcd_spi_transfer(TRUE, 1, &SLEEP);
        close(spi_fd);
    }
    if (DnC_PIN) {
        gpio_close(DnC_PIN);
    }
    if (RESET_PIN1) {
        gpio_close(RESET_PIN1);
    }
    if (RESET_PIN2) {
        gpio_close(RESET_PIN2);
    }
}

