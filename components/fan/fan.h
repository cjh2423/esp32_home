#ifndef FAN_H
#define FAN_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化风扇控制
 * 
 * @param fan_gpio 风扇GPIO
 * @param fan_channel 风扇PWM通道
 * @param unused_gpio 加湿器GPIO (已废弃，传任意值)
 * @return esp_err_t 
 */
esp_err_t fan_init(uint8_t fan_gpio, uint8_t fan_channel, int8_t unused_gpio);

/**
 * @brief 设置风扇速度
 * 
 * @param speed 0-255
 * @return esp_err_t 
 */
esp_err_t fan_set_speed(uint8_t speed);

// 移除 humidifier_control

#endif // FAN_H
