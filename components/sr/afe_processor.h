/**
 * @file afe_processor.h
 * @brief ESP AFE (Audio Front-End) 音频处理器封装
 *
 * 提供降噪(NS)、语音活动检测(VAD)、唤醒词检测(WakeNet)等功能
 * 参考 xiaozhi-esp32 afe_wake_word.cc 实现
 */

#ifndef AFE_PROCESSOR_H
#define AFE_PROCESSOR_H

#include "esp_err.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AFE 处理器句柄
 */
typedef struct afe_processor* afe_processor_handle_t;

// 注意：afe_fetch_result_t 已在 esp_afe_sr_iface.h 中定义
// 包含 data, data_size, vad_state, wakeup_state 等字段

/**
 * @brief VAD 灵敏度模式
 */
#define AFE_VAD_MODE_LEAST_SENSITIVE  VAD_MODE_0
#define AFE_VAD_MODE_MOST_SENSITIVE   VAD_MODE_4

/**
 * @brief AFE 配置结构
 */
typedef struct {
    bool enable_ns;             // 启用降噪
    bool enable_vad;            // 启用 VAD
    bool enable_wakenet;        // 启用唤醒词检测 (使用 AFE_TYPE_SR)
    bool enable_agc;            // 启用 AGC (通常关闭)
    bool use_psram;             // 使用 PSRAM 分配内存
    int vad_mode;               // VAD 灵敏度模式 (VAD_MODE_0 ~ VAD_MODE_4)
    int vad_min_noise_ms;       // VAD 最小静音时间 (ms)
    int afe_perferred_core;     // AFE 首选 CPU 核心
    int afe_perferred_priority; // AFE 首选优先级
} afe_processor_config_t;

/**
 * @brief 默认 AFE 配置 (启用 WakeNet，参考 xiaozhi)
 */
#define AFE_PROCESSOR_CONFIG_DEFAULT() { \
    .enable_ns = false,                   \
    .enable_vad = true,                  \
    .enable_wakenet = true,              \
    .enable_agc = false,                 \
    .use_psram = true,                   \
    .vad_mode = AFE_VAD_MODE_MOST_SENSITIVE, \
    .vad_min_noise_ms = 50,              \
    .afe_perferred_core = 1,             \
    .afe_perferred_priority = 1          \
}

/**
 * @brief 创建 AFE 处理器
 *
 * @param config AFE 配置
 * @param models SR 模型列表 (必须提供，用于 WakeNet)
 * @return afe_processor_handle_t 句柄，失败返回 NULL
 */
afe_processor_handle_t afe_processor_create(const afe_processor_config_t *config, srmodel_list_t *models);

/**
 * @brief 销毁 AFE 处理器
 */
void afe_processor_destroy(afe_processor_handle_t handle);

/**
 * @brief 获取 feed 数据块大小
 */
size_t afe_processor_get_feed_chunksize(afe_processor_handle_t handle);

/**
 * @brief 获取 fetch 数据块大小
 */
size_t afe_processor_get_fetch_chunksize(afe_processor_handle_t handle);

/**
 * @brief 喂入原始音频数据
 */
esp_err_t afe_processor_feed(afe_processor_handle_t handle, int16_t *data);

/**
 * @brief 获取处理后的音频数据 (阻塞，包含唤醒状态)
 *
 * @param handle AFE 句柄
 * @param result 输出结果结构体
 * @param timeout_ms 超时时间 (portMAX_DELAY 表示无限等待)
 * @return esp_err_t ESP_OK 成功, ESP_ERR_TIMEOUT 超时
 */
esp_err_t afe_processor_fetch_ex(afe_processor_handle_t handle,
                                  afe_fetch_result_t *result,
                                  uint32_t timeout_ms);

/**
 * @brief 获取处理后的音频数据 (简化版，兼容旧接口)
 */
esp_err_t afe_processor_fetch(afe_processor_handle_t handle, int16_t **out_data,
                               afe_vad_state_t *out_vad, uint32_t timeout_ms);

/**
 * @brief 重置 AFE 缓冲区
 */
void afe_processor_reset(afe_processor_handle_t handle);

/**
 * @brief 获取采样率 (固定 16000)
 */
int afe_processor_get_sample_rate(afe_processor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // AFE_PROCESSOR_H
