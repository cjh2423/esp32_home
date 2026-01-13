#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 语音命令枚举
 */
typedef enum {
    VR_CMD_WAKE_UP = 0,     // 唤醒
    VR_CMD_LIGHT_ON,        // 打开灯光
    VR_CMD_LIGHT_OFF,       // 关闭灯光
    VR_CMD_FAN_ON,          // 打开风扇
    VR_CMD_FAN_OFF,         // 关闭风扇
    VR_CMD_UNKNOWN          // 未知命令
} vr_command_t;

/**
 * @brief 语音命令回调函数类型
 * 
 * @param command 识别到的命令
 */
typedef void (*vr_command_callback_t)(vr_command_t command);

/**
 * @brief 初始化语音识别模块
 * 
 * @param sck_io I2S 时钟引脚
 * @param ws_io I2S 字选择引脚
 * @param sd_io I2S 数据引脚
 * @param callback 命令回调函数
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t vr_init(int sck_io, int ws_io, int sd_io, vr_command_callback_t callback);

/**
 * @brief 启动语音识别任务
 * 
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t vr_start(void);

/**
 * @brief 停止语音识别任务
 * 
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t vr_stop(void);

/**
 * @brief 反初始化语音识别模块
 * 
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t vr_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // VOICE_RECOGNITION_H
