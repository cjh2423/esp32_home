#ifndef BH1750_DRIVER_H
#define BH1750_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化 BH1750 光照传感器 (使用 esp-idf-lib/bh1750 组件)
 *
 * @param sda_gpio SDA引脚
 * @param scl_gpio SCL引脚
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t bh1750_sensor_init(uint8_t sda_gpio, uint8_t scl_gpio);

/**
 * @brief 读取 BH1750 光照强度
 *
 * @param lux 输出光照强度（lux）
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t bh1750_sensor_read(float *lux);

/**
 * @brief 反初始化 BH1750
 */
void bh1750_sensor_deinit(void);

#endif // BH1750_DRIVER_H
