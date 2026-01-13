#ifndef SERVO_DRIVER_H
#define SERVO_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化舵机控制 (使用 espressif/servo 官方组件)
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

/**
 * @brief 设置舵机角度
 *
 * @param angle 角度 (0-180)
 * @return esp_err_t
 */
esp_err_t servo_set_angle(float angle);

/**
 * @brief 反初始化舵机
 */
void motor_deinit(void);

#endif // SERVO_DRIVER_H
