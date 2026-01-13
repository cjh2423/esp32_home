#ifndef INMP441_DRIVER_H
#define INMP441_DRIVER_H

#include "esp_err.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 INMP441 麦克风驱动
 * 
 * @param sck_io I2S 时钟引脚
 * @param ws_io I2S 字选择引脚
 * @param sd_io I2S 数据引脚
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t inmp441_init(int sck_io, int ws_io, int sd_io);

/**
 * @brief 从 INMP441 读取音频数据
 * 
 * @param buffer 数据缓冲区
 * @param buffer_size 缓冲区大小（字节）
 * @param bytes_read 实际读取的字节数
 * @param timeout_ms 超时时间（毫秒）
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t inmp441_read(void *buffer, size_t buffer_size, size_t *bytes_read, uint32_t timeout_ms);

/**
 * @brief 反初始化 INMP441 驱动
 * 
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t inmp441_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // INMP441_DRIVER_H
