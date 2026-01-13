/**
 * @file rgb_led.h
 * @brief ESP32-S3 板载 RGB LED 驱动 (GPIO48, WS2812)
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 预定义颜色
 */
typedef enum {
    RGB_COLOR_OFF = 0,
    RGB_COLOR_RED,
    RGB_COLOR_GREEN,
    RGB_COLOR_BLUE,
    RGB_COLOR_YELLOW,
    RGB_COLOR_CYAN,
    RGB_COLOR_MAGENTA,
    RGB_COLOR_WHITE,
    RGB_COLOR_ORANGE,
    RGB_COLOR_PURPLE,
} rgb_color_t;

/**
 * @brief 初始化 RGB LED
 *
 * @param gpio_num GPIO 引脚号 (ESP32-S3 核心板通常是 GPIO48)
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t rgb_led_init(int gpio_num);

/**
 * @brief 设置 RGB LED 颜色 (RGB 值)
 *
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t rgb_led_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 设置预定义颜色
 *
 * @param color 预定义颜色枚举
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t rgb_led_set_color(rgb_color_t color);

/**
 * @brief 设置 RGB LED 亮度
 *
 * @param brightness 亮度 (0-100)
 */
void rgb_led_set_brightness(uint8_t brightness);

/**
 * @brief 关闭 RGB LED
 *
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t rgb_led_off(void);

/**
 * @brief 闪烁 RGB LED
 *
 * @param color 颜色
 * @param times 闪烁次数
 * @param interval_ms 闪烁间隔 (毫秒)
 */
void rgb_led_blink(rgb_color_t color, int times, int interval_ms);

/**
 * @brief 反初始化 RGB LED
 */
void rgb_led_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // RGB_LED_H
