#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <string.h>
#include <ssd1306/ssd1306.h>

#define LOAD_ICON_X 54
#define LOAD_ICON_Y 32
#define LOAD_ICON_SIZE 20

#define CIRCLE_COUNT_ICON_X 94
#define CIRCLE_COUNT_ICON_Y 42

/* Remove this line if your display connected by SPI */
//#define I2C_CONNECTION

#ifdef I2C_CONNECTION
    #include <i2c/i2c.h>
#endif
#include "fonts/fonts.h"

/* Change this according to you schematics and display size */
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

#ifdef I2C_CONNECTION
    #define PROTOCOL SSD1306_PROTO_I2C
    #define ADDR     SSD1306_I2C_ADDR_0
    #define SCL_PIN  14
    #define SDA_PIN  13
#else
    #define PROTOCOL SSD1306_PROTO_SPI4
    #define CS_PIN   5
    #define DC_PIN   4
#endif

/* Declare device descriptor */
static const ssd1306_t dev = {
    .protocol = PROTOCOL,
#ifdef I2C_CONNECTION
    .addr     = ADDR,
#else
    .cs_pin   = CS_PIN,
    .dc_pin   = DC_PIN,
#endif
    .width    = DISPLAY_WIDTH,
    .height   = DISPLAY_HEIGHT
};

/* Local frame buffer */
static uint8_t buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

TimerHandle_t timerHandle = 0; // Timer handler
uint8_t frame_done = 0; // number of frame send.
uint8_t fps = 0; // image per second.

#define SECOND (1000 / portTICK_PERIOD_MS)

static void ssd1306_task(void *pvParameters)
{
    printf("%s: Started user interface task\n", __FUNCTION__);
    vTaskDelay(SECOND);

    ssd1306_set_whole_display_lighting(&dev, false);

    char text[20];
    uint8_t x0 = LOAD_ICON_X;
    uint8_t y0 = LOAD_ICON_Y;
    uint8_t x1 = LOAD_ICON_X + LOAD_ICON_SIZE;
    uint8_t y1 = LOAD_ICON_Y + LOAD_ICON_SIZE;
    uint16_t count = 0;
    uint8_t font = 0;
    //ssd1306_select_font(FONT_FACE_TERMINUS_BOLD_8X14_KOI8_R);

    while (1) {

        if (!(count % 200))
            while (true)
            {
                if (font++ >= FONT_FACE_MAX) font = 0;
                if (!ssd1306_select_font(font)) break;
            }

        ssd1306_draw_string(&dev, buffer, 10, 10,"Hello, esp-open-rtos !", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        sprintf(text,"FPS: %u",fps);
        ssd1306_draw_string(&dev, buffer, 10, 40, text, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

        //generate loading icon
        ssd1306_draw_line(&dev, buffer, x0, y0, x1, y1, OLED_COLOR_BLACK);
        if (x0 < (LOAD_ICON_X + LOAD_ICON_SIZE)) {
            x0++;
            x1--;
        }
        else if (y0 < (LOAD_ICON_Y + LOAD_ICON_SIZE)) {
            y0++;
            y1--;
        }
        else {
            x0 = LOAD_ICON_X;
            y0 = LOAD_ICON_Y;
            x1 = LOAD_ICON_X + LOAD_ICON_SIZE;
            y1 = LOAD_ICON_Y + LOAD_ICON_SIZE;
        }
        ssd1306_draw_line(&dev, buffer, x0, y0, x1, y1, OLED_COLOR_WHITE);
        ssd1306_draw_rectangle(&dev, buffer, LOAD_ICON_X, LOAD_ICON_Y,
            LOAD_ICON_SIZE + 1, LOAD_ICON_SIZE + 1, OLED_COLOR_WHITE);

        //generate circle counting
        for (uint8_t i = 0; i < 10; i++) {
            if ((count >> i) & 0x01)
                ssd1306_draw_circle(&dev, buffer, CIRCLE_COUNT_ICON_X, CIRCLE_COUNT_ICON_Y, i, OLED_COLOR_BLACK);
        }

        count = count == 0x03FF ? 0 : count + 1;

        for (uint8_t i = 0; i < 10; i++) {
            if ((count>>i) & 0x01)
                ssd1306_draw_circle(&dev,buffer, CIRCLE_COUNT_ICON_X, CIRCLE_COUNT_ICON_Y, i, OLED_COLOR_WHITE);
        }

        if (ssd1306_load_frame_buffer(&dev, buffer))
            goto error_loop;

        frame_done++;
    }

error_loop:
    printf("%s: error while loading framebuffer into SSD1306\n", __func__);
    for (;;) {
        vTaskDelay(2 * SECOND);
        printf("%s: error loop\n", __FUNCTION__);
    }
}

void SoftTimer(TimerHandle_t xTimer)
{
    fps = frame_done; // Save number of frame already send to screen
    frame_done = 0;
}

void user_init(void)
{
    //uncomment to test with CPU overclocked
    //sdk_system_update_cpu_freq(160);

    // Setup HW
    uart_set_baud(0, 115200);

    printf("SDK version:%s\n", sdk_system_get_sdk_version());

#ifdef I2C_CONNECTION
    i2c_init(SCL_PIN, SDA_PIN);
#endif

    while (ssd1306_init(&dev) != 0) {
        printf("%s: failed to init SSD1306 lcd\n", __func__);
        vTaskDelay(SECOND);
    }
    ssd1306_set_whole_display_lighting(&dev, true);
    vTaskDelay(SECOND);

    // Create user interface task
    xTaskCreate(ssd1306_task, "ssd1306_task", 256, NULL, 2, NULL);

    timerHandle = xTimerCreate("Timer", SECOND, pdTRUE, NULL, SoftTimer);
    xTimerStart(timerHandle, 0);
}
