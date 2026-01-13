/**
 * @file afe_processor.h
 * @brief ESP AFE (Audio Front-End) 音频处理器封装
 *
 * 提供降噪(NS)、语音活动检测(VAD)等音频前端处理功能
 * 参考 xiaozhi-esp32 实现
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

// 使用 SDK 定义的 afe_vad_state_t (AFE_VAD_SILENCE, AFE_VAD_SPEECH)

/**
 * @brief AFE fetch 结果回调
 *
 * @param data 处理后的音频数据 (16-bit PCM)
 * @param len 数据长度 (样本数)
 * @param vad_state VAD 状态
 */
typedef void (*afe_output_callback_t)(int16_t *data, size_t len, afe_vad_state_t vad_state);

/**
 * @brief VAD 状态变化回调
 *
 * @param speaking true=语音开始, false=语音结束
 */
typedef void (*afe_vad_callback_t)(bool speaking);

/**
 * @brief AFE 处理器句柄
 */
typedef struct afe_processor* afe_processor_handle_t;

/**
 * @brief VAD 灵敏度模式
 *
 * VAD_MODE_0: 最不敏感 (高阈值，减少误触发)
 * VAD_MODE_4: 最敏感 (低阈值，更容易检测到语音)
 */
#define AFE_VAD_MODE_LEAST_SENSITIVE  VAD_MODE_0
#define AFE_VAD_MODE_MOST_SENSITIVE   VAD_MODE_4

/**
 * @brief AFE 配置结构
 */
typedef struct {
    bool enable_ns;         // 启用降噪
    bool enable_vad;        // 启用 VAD
    bool enable_agc;        // 启用 AGC (通常关闭)
    bool use_psram;         // 使用 PSRAM 分配内存
    int vad_mode;           // VAD 灵敏度模式 (VAD_MODE_0 ~ VAD_MODE_4)
    int vad_min_noise_ms;   // VAD 最小静音时间 (ms)
} afe_processor_config_t;

/**
 * @brief 默认 AFE 配置
 */
#define AFE_PROCESSOR_CONFIG_DEFAULT() { \
    .enable_ns = true,                   \
    .enable_vad = true,                  \
    .enable_agc = false,                 \
    .use_psram = true,                   \
    .vad_mode = AFE_VAD_MODE_MOST_SENSITIVE, \
    .vad_min_noise_ms = 50               \
}

/**
 * @brief 创建 AFE 处理器
 *
 * @param config AFE 配置
 * @param models SR 模型列表 (可为 NULL，将自动加载)
 * @return afe_processor_handle_t 句柄，失败返回 NULL
 */
afe_processor_handle_t afe_processor_create(const afe_processor_config_t *config, srmodel_list_t *models);

/**
 * @brief 销毁 AFE 处理器
 *
 * @param handle AFE 句柄
 */
void afe_processor_destroy(afe_processor_handle_t handle);

/**
 * @brief 获取 feed 数据块大小
 *
 * @param handle AFE 句柄
 * @return size_t 每次 feed 所需的样本数
 */
size_t afe_processor_get_feed_chunksize(afe_processor_handle_t handle);

/**
 * @brief 获取 fetch 数据块大小
 *
 * @param handle AFE 句柄
 * @return size_t 每次 fetch 输出的样本数
 */
size_t afe_processor_get_fetch_chunksize(afe_processor_handle_t handle);

/**
 * @brief 喂入原始音频数据
 *
 * @param handle AFE 句柄
 * @param data 音频数据 (16-bit PCM)
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t afe_processor_feed(afe_processor_handle_t handle, int16_t *data);

/**
 * @brief 获取处理后的音频数据 (阻塞)
 *
 * @param handle AFE 句柄
 * @param out_data 输出缓冲区
 * @param out_vad VAD 状态输出
 * @param timeout_ms 超时时间
 * @return esp_err_t ESP_OK 成功, ESP_ERR_TIMEOUT 超时
 */
esp_err_t afe_processor_fetch(afe_processor_handle_t handle, int16_t **out_data,
                               afe_vad_state_t *out_vad, uint32_t timeout_ms);

/**
 * @brief 重置 AFE 缓冲区
 *
 * @param handle AFE 句柄
 */
void afe_processor_reset(afe_processor_handle_t handle);

/**
 * @brief 获取采样率
 *
 * @param handle AFE 句柄
 * @return int 采样率 (通常 16000)
 */
int afe_processor_get_sample_rate(afe_processor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // AFE_PROCESSOR_H
