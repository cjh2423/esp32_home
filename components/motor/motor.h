#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化步进电机
 * 
 * @param step_gpio 步进脉冲引脚
 * @param dir_gpio 方向引脚
 * @param en_gpio 使能引脚
 * @return esp_err_t 
 */
esp_err_t motor_init(uint8_t step_gpio, uint8_t dir_gpio, uint8_t en_gpio);

/**
 * @brief 控制窗帘开关
 * 
 * @param open 1-打开窗帘，0-关闭窗帘
 * @return esp_err_t 
 */
esp_err_t curtain_control(uint8_t open);

#endif // MOTOR_H
