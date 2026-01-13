#ifndef DHT_DRIVER_H
#define DHT_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

// DHT11 数据结构 
typedef struct {
    float temperature;  // 温度（℃）
    float humidity;     // 湿度（%）
    uint8_t valid;      // 数据有效标志
} dht11_data_t;

/**
 * @brief 初始化 DHT11 传感器 (使用 esp-idf-lib/dht 组件)
 *
 * @param gpio_num GPIO 引脚号
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t dht11_init(uint8_t gpio_num);

/**
 * @brief 读取 DHT11 传感器数据
 *
 * @param data 输出数据结构指针
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t dht11_read(dht11_data_t *data);

/**
 * @brief 反初始化 DHT11 (重置状态)
 */
void dht11_deinit(void);

#endif // DHT_DRIVER_H
