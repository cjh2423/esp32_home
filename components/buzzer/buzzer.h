#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化蜂鸣器
 * 
 * @param gpio_num GPIO引脚
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t buzzer_init(uint8_t gpio_num);

/**
 * @brief 蜂鸣器响
 * 
 * @param gpio_num GPIO引脚
 * @param duration_ms 持续时间（毫秒）
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t buzzer_beep(uint8_t gpio_num, uint32_t duration_ms);

/**
 * @brief 蜂鸣器报警（连续响）
 * 
 * @param gpio_num GPIO引脚
 * @param times 响的次数
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t buzzer_alarm(uint8_t gpio_num, uint8_t times);

#endif // BUZZER_H
