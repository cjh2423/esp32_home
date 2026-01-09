#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化 LED PWM 控制
 * 
 * @param gpio_num GPIO引脚
 * @param channel PWM通道
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t led_init(uint8_t gpio_num, uint8_t channel);

/**
 * @brief 设置 LED 亮度
 * 
 * @param channel PWM通道
 * @param brightness 亮度值（0-255）
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t led_set_brightness(uint8_t channel, uint8_t brightness);

/**
 * @brief 打开 LED
 * 
 * @param channel PWM通道
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t led_on(uint8_t channel);

/**
 * @brief 关闭 LED
 * 
 * @param channel PWM通道
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t led_off(uint8_t channel);

#endif // LED_H
