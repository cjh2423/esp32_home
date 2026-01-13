/**
 * @file rgb_led.c
 * @brief ESP32-S3 板载 RGB LED 驱动实现
 *
 * 使用 Espressif led_strip 组件驱动 WS2812 RGB LED
 */

#include "rgb_led.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RGB_LED";

static led_strip_handle_t s_led_strip = NULL;
static uint8_t s_brightness = 50;  // 默认亮度 50%

// 预定义颜色表 (R, G, B)
static const uint8_t s_color_table[][3] = {
    [RGB_COLOR_OFF]     = {0, 0, 0},
    [RGB_COLOR_RED]     = {255, 0, 0},
    [RGB_COLOR_GREEN]   = {0, 255, 0},
    [RGB_COLOR_BLUE]    = {0, 0, 255},
    [RGB_COLOR_YELLOW]  = {255, 255, 0},
    [RGB_COLOR_CYAN]    = {0, 255, 255},
    [RGB_COLOR_MAGENTA] = {255, 0, 255},
    [RGB_COLOR_WHITE]   = {255, 255, 255},
    [RGB_COLOR_ORANGE]  = {255, 128, 0},
    [RGB_COLOR_PURPLE]  = {128, 0, 255},
};

esp_err_t rgb_led_init(int gpio_num)
{
    // 配置 LED Strip (RMT 后端，适合 WS2812)
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = 1,  // 单个 RGB LED
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10 MHz
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化为关闭状态
    led_strip_clear(s_led_strip);

    ESP_LOGI(TAG, "RGB LED initialized on GPIO%d", gpio_num);
    return ESP_OK;
}

esp_err_t rgb_led_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // 应用亮度
    uint8_t r = (uint8_t)((red * s_brightness) / 100);
    uint8_t g = (uint8_t)((green * s_brightness) / 100);
    uint8_t b = (uint8_t)((blue * s_brightness) / 100);

    esp_err_t ret = led_strip_set_pixel(s_led_strip, 0, r, g, b);
    if (ret != ESP_OK) {
        return ret;
    }

    return led_strip_refresh(s_led_strip);
}

esp_err_t rgb_led_set_color(rgb_color_t color)
{
    if (color >= sizeof(s_color_table) / sizeof(s_color_table[0])) {
        return ESP_ERR_INVALID_ARG;
    }

    return rgb_led_set_rgb(
        s_color_table[color][0],
        s_color_table[color][1],
        s_color_table[color][2]
    );
}

void rgb_led_set_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        brightness = 100;
    }
    s_brightness = brightness;
}

esp_err_t rgb_led_off(void)
{
    if (s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = led_strip_clear(s_led_strip);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

void rgb_led_blink(rgb_color_t color, int times, int interval_ms)
{
    for (int i = 0; i < times; i++) {
        rgb_led_set_color(color);
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
        rgb_led_off();
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
}

void rgb_led_deinit(void)
{
    if (s_led_strip != NULL) {
        led_strip_clear(s_led_strip);
        led_strip_del(s_led_strip);
        s_led_strip = NULL;
        ESP_LOGI(TAG, "RGB LED deinitialized");
    }
}
