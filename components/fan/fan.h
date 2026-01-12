#ifndef FAN_H
#define FAN_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化风扇控制
 * 
 * @param fan_gpio 风扇GPIO
 * @param fan_channel 风扇PWM通道
 * @return esp_err_t 
 */
esp_err_t fan_init(uint8_t fan_gpio, uint8_t fan_channel);

/**
 * @brief 设置风扇速度
 *
 * @param speed 0-255
 * @return esp_err_t
 */
esp_err_t fan_set_speed(uint8_t speed);

/**
 * @brief 释放风扇 PWM 资源
 *
 * @return esp_err_t
 */
esp_err_t fan_deinit(void);

// 移除 humidifier_control

#endif // FAN_H
