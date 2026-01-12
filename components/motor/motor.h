#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化舵机控制
 *
 * @param servo_gpio 舵机PWM引脚
 * @return esp_err_t 
 */
esp_err_t motor_init(uint8_t servo_gpio);

/**
 * @brief 控制窗帘开关
 * 
 * @param open 1-打开窗帘，0-关闭窗帘
 * @return esp_err_t 
 */
esp_err_t curtain_control(uint8_t open);

#endif // MOTOR_H
