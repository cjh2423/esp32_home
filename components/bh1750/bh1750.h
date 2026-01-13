#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// BH1750 I2C地址
#define BH1750_ADDR 0x23

// BH1750 指令
#define BH1750_POWER_ON 0x01
#define BH1750_POWER_OFF 0x00
#define BH1750_RESET 0x07
#define BH1750_CONTINUOUS_HIGH_RES_MODE 0x10

/**
 * @brief 初始化 BH1750 光照传感器
 *
 * @param sda_gpio SDA引脚
 * @param scl_gpio SCL引脚
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t bh1750_init(uint8_t sda_gpio, uint8_t scl_gpio);

/**
 * @brief 读取 BH1750 光照强度
 *
 * @param lux 输出光照强度（lux）
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t bh1750_read(float *lux);

/**
 * @brief 反初始化 BH1750
 */
void bh1750_deinit(void);

#endif // BH1750_H
