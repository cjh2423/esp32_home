#ifndef MQ2_H
#define MQ2_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/adc.h"

/**
 * @brief 初始化 MQ-2 烟雾传感器
 * 
 * @param adc_channel ADC通道
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t mq2_init(adc1_channel_t adc_channel);

/**
 * @brief 读取 MQ-2 传感器值
 * 
 * @param adc_channel ADC通道
 * @param value 输出ADC值（0-4095）
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t mq2_read(adc1_channel_t adc_channel, uint32_t *value);

/**
 * @brief 判断是否检测到烟雾
 * 
 * @param value ADC值
 * @param threshold 阈值
 * @return uint8_t 1-检测到烟雾，0-正常
 */
uint8_t mq2_is_smoke_detected(uint32_t value, uint32_t threshold);

#endif // MQ2_H
