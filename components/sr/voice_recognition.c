#include "voice_recognition.h"
#include "inmp441_driver.h"
#include "afe_processor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

// ESP-SR 库头文件
#include "esp_afe_sr_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"

static const char *TAG = "VR";

// 语音命令拼音定义
static const char *s_commands[] = {
    "da kai deng guang",    // ID 1: 打开灯光
    "guan bi deng guang",   // ID 2: 关闭灯光
    "da kai feng shan",     // ID 3: 打开风扇
    "guan bi feng shan",    // ID 4: 关闭风扇
};
#define NUM_COMMANDS (sizeof(s_commands) / sizeof(s_commands[0]))

// 全局变量
static vr_command_callback_t s_callback = NULL;
static TaskHandle_t s_task_handle = NULL;
static volatile bool s_task_running = false;  // 任务运行标志

// ESP-SR 模型句柄
static srmodel_list_t *s_models = NULL;
static const esp_wn_iface_t *s_wn_iface = NULL;
static model_iface_data_t *s_wn_model = NULL;
static const esp_mn_iface_t *s_mn_iface = NULL;
static model_iface_data_t *s_mn_model = NULL;

// AFE 处理器
static afe_processor_handle_t s_afe = NULL;

// 状态管理
typedef enum {
    VR_STATE_WAITING_WAKE,
    VR_STATE_WAITING_COMMAND
} vr_state_t;

static vr_state_t s_state = VR_STATE_WAITING_WAKE;

/**
 * @brief 将命令 ID 映射为枚举
 */
static vr_command_t map_command_id(int id)
{
    switch (id) {
        case 1: return VR_CMD_LIGHT_ON;
        case 2: return VR_CMD_LIGHT_OFF;
        case 3: return VR_CMD_FAN_ON;
        case 4: return VR_CMD_FAN_OFF;
        default: return VR_CMD_UNKNOWN;
    }
}

/**
 * @brief 初始化 ESP-SR 模型
 */
static esp_err_t init_sr_models(void)
{
    s_models = esp_srmodel_init("model");
    if (s_models == NULL) {
        ESP_LOGE(TAG, "Failed to init SR model list");
        return ESP_FAIL;
    }

    // 创建 AFE 处理器 (NS + VAD)
    afe_processor_config_t afe_cfg = AFE_PROCESSOR_CONFIG_DEFAULT();
    s_afe = afe_processor_create(&afe_cfg, s_models);
    if (s_afe == NULL) {
        ESP_LOGE(TAG, "Failed to create AFE processor");
        goto err_cleanup_models;
    }

    // 加载 WakeNet 模型
    char *wn_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, SR_WAKENET_MODEL);
    if (wn_name == NULL) {
        ESP_LOGE(TAG, "Failed to find WakeNet model: %s", SR_WAKENET_MODEL);
        goto err_cleanup_afe;
    }

    s_wn_iface = esp_wn_handle_from_name(wn_name);
    if (s_wn_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get WakeNet interface");
        goto err_cleanup_afe;
    }

    s_wn_model = s_wn_iface->create(wn_name, SR_WAKENET_MODE);
    if (s_wn_model == NULL) {
        ESP_LOGE(TAG, "Failed to create WakeNet model");
        goto err_cleanup_afe;
    }

    ESP_LOGI(TAG, "WakeNet ready (SR: %d Hz, Chunk: %d)",
             s_wn_iface->get_samp_rate(s_wn_model),
             s_wn_iface->get_samp_chunksize(s_wn_model));

    // 验证 AFE fetch chunksize 与 WakeNet chunksize 匹配
    size_t afe_fetch = afe_processor_get_fetch_chunksize(s_afe);
    int wn_chunk = s_wn_iface->get_samp_chunksize(s_wn_model);
    if (afe_fetch != (size_t)wn_chunk) {
        ESP_LOGW(TAG, "Chunksize mismatch: AFE fetch=%u, WakeNet=%d",
                 (unsigned)afe_fetch, wn_chunk);
    }

    // 加载 MultiNet 模型
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, SR_MULTINET_MODEL);
    if (mn_name == NULL) {
        ESP_LOGE(TAG, "Failed to find MultiNet model: %s", SR_MULTINET_MODEL);
        goto err_cleanup_wn;
    }

    s_mn_iface = esp_mn_handle_from_name(mn_name);
    if (s_mn_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get MultiNet interface");
        goto err_cleanup_wn;
    }

    s_mn_model = s_mn_iface->create(mn_name, 5000);
    if (s_mn_model == NULL) {
        ESP_LOGE(TAG, "Failed to create MultiNet model");
        goto err_cleanup_wn;
    }

    // 注册自定义命令
    esp_mn_commands_clear();
    for (int i = 0; i < NUM_COMMANDS; i++) {
        esp_mn_commands_add(i + 1, (char *)s_commands[i]);
    }

    esp_mn_error_t *err = esp_mn_commands_update();
    if (err) {
        ESP_LOGE(TAG, "Failed to update commands");
        goto err_cleanup_mn;
    }

    ESP_LOGI(TAG, "MultiNet ready (%d commands)", (int)NUM_COMMANDS);
    return ESP_OK;

err_cleanup_mn:
    if (s_mn_iface && s_mn_model) {
        s_mn_iface->destroy(s_mn_model);
        s_mn_model = NULL;
    }
err_cleanup_wn:
    if (s_wn_iface && s_wn_model) {
        s_wn_iface->destroy(s_wn_model);
        s_wn_model = NULL;
    }
err_cleanup_afe:
    if (s_afe) {
        afe_processor_destroy(s_afe);
        s_afe = NULL;
    }
err_cleanup_models:
    // 模型列表由 SDK 管理
    s_models = NULL;
    return ESP_FAIL;
}

/**
 * @brief 语音识别主任务 (使用 AFE 前端处理)
 */
static void vr_task(void *arg)
{
    size_t feed_chunksize = afe_processor_get_feed_chunksize(s_afe);
    size_t fetch_chunksize = afe_processor_get_fetch_chunksize(s_afe);

    ESP_LOGI(TAG, "Task started (AFE feed: %u, fetch: %u)",
             (unsigned)feed_chunksize, (unsigned)fetch_chunksize);

    // 分配 I2S 读取缓冲区 (32-bit from INMP441)
    int32_t *i2s_buffer = (int32_t *)malloc(feed_chunksize * sizeof(int32_t));
    if (i2s_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate I2S buffer");
        s_task_running = false;
        vTaskDelete(NULL);
        return;
    }

    // 分配 16-bit 音频缓冲区供 AFE feed
    int16_t *audio_buffer = (int16_t *)malloc(feed_chunksize * sizeof(int16_t));
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        free(i2s_buffer);
        s_task_running = false;
        vTaskDelete(NULL);
        return;
    }

    while (s_task_running) {
        size_t bytes_read = 0;

        // 从 INMP441 读取数据 (使用短超时以便检查停止标志)
        esp_err_t ret = inmp441_read(i2s_buffer, feed_chunksize * sizeof(int32_t),
                                      &bytes_read, 100);
        if (ret == ESP_ERR_TIMEOUT) {
            continue;  // 超时，检查停止标志
        }
        if (ret != ESP_OK || bytes_read != feed_chunksize * sizeof(int32_t)) {
            ESP_LOGW(TAG, "I2S read failed or incomplete");
            continue;
        }

        // 转换 32 位数据到 16 位（INMP441 24 位数据存储在高 24 位）
        for (size_t i = 0; i < feed_chunksize; i++) {
            audio_buffer[i] = (int16_t)(i2s_buffer[i] >> 16);
        }

        // 喂入 AFE 处理器
        afe_processor_feed(s_afe, audio_buffer);

        // 获取 AFE 处理后的音频
        int16_t *afe_output = NULL;
        afe_vad_state_t vad_state = AFE_VAD_SILENCE;

        ret = afe_processor_fetch(s_afe, &afe_output, &vad_state, 100);
        if (ret != ESP_OK || afe_output == NULL) {
            continue;  // 超时或无数据
        }

        // 仅在 VAD 检测到语音时进行识别 (节省 CPU)
        if (vad_state == AFE_VAD_SILENCE && s_state == VR_STATE_WAITING_WAKE) {
            continue;  // 静音时跳过唤醒词检测
        }

        // 根据状态处理音频
        if (s_state == VR_STATE_WAITING_WAKE) {
            wakenet_state_t wn_state = s_wn_iface->detect(s_wn_model, afe_output);

            if (wn_state > 0) {
                ESP_LOGI(TAG, "Wake word detected!");
                s_state = VR_STATE_WAITING_COMMAND;
                s_mn_iface->clean(s_mn_model);

                if (s_callback) {
                    s_callback(VR_CMD_WAKE_UP);
                }
            }
        }
        else if (s_state == VR_STATE_WAITING_COMMAND) {
            esp_mn_state_t mn_state = s_mn_iface->detect(s_mn_model, afe_output);

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *result = s_mn_iface->get_results(s_mn_model);
                if (result && result->num > 0) {
                    int cmd_id = result->phrase_id[0];
                    ESP_LOGI(TAG, "Command detected: ID %d", cmd_id);

                    vr_command_t cmd = map_command_id(cmd_id);
                    if (s_callback && cmd != VR_CMD_UNKNOWN) {
                        s_callback(cmd);
                    }

                    s_mn_iface->clean(s_mn_model);
                }
                // 命令识别成功后返回唤醒等待状态
                s_state = VR_STATE_WAITING_WAKE;
            }
            else if (mn_state == ESP_MN_STATE_TIMEOUT) {
                ESP_LOGI(TAG, "Command timeout, back to wake mode");
                s_state = VR_STATE_WAITING_WAKE;
            }
        }
    }

    // 清理资源
    free(audio_buffer);
    free(i2s_buffer);
    ESP_LOGI(TAG, "Task stopped");
    vTaskDelete(NULL);
}

esp_err_t vr_init(int sck_io, int ws_io, int sd_io, vr_command_callback_t callback)
{
    if (callback == NULL) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    s_callback = callback;

    // 初始化 INMP441
    esp_err_t ret = inmp441_init(sck_io, ws_io, sd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init INMP441");
        return ret;
    }

    // 初始化 SR 模型
    ret = init_sr_models();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SR models");
        inmp441_deinit();
        return ret;
    }

    ESP_LOGI(TAG, "Voice recognition initialized");
    return ESP_OK;
}

esp_err_t vr_start(void)
{
    if (s_task_handle != NULL) {
        ESP_LOGW(TAG, "Task already running");
        return ESP_OK;
    }

    s_task_running = true;
    s_state = VR_STATE_WAITING_WAKE;

    BaseType_t ret = xTaskCreatePinnedToCore(
        vr_task,
        "vr_task",
        16 * 1024,  // 16KB 栈空间
        NULL,
        5,
        &s_task_handle,
        1  // 绑定到核心 1
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        s_task_running = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t vr_stop(void)
{
    if (s_task_handle == NULL) {
        return ESP_OK;
    }

    // 设置停止标志，等待任务自行退出
    s_task_running = false;

    // 等待任务退出 (最多 500ms)
    for (int i = 0; i < 50 && s_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        // 检查任务是否已删除自己
        if (eTaskGetState(s_task_handle) == eDeleted) {
            break;
        }
    }

    s_task_handle = NULL;

    // 重置状态机，确保下次启动从唤醒等待状态开始
    s_state = VR_STATE_WAITING_WAKE;

    // 重置 AFE 缓冲，清除残留数据
    if (s_afe) {
        afe_processor_reset(s_afe);
    }

    // 清理 MultiNet 状态
    if (s_mn_iface && s_mn_model) {
        s_mn_iface->clean(s_mn_model);
    }

    return ESP_OK;
}

esp_err_t vr_deinit(void)
{
    vr_stop();

    if (s_wn_iface && s_wn_model) {
        s_wn_iface->destroy(s_wn_model);
        s_wn_model = NULL;
    }

    if (s_mn_iface && s_mn_model) {
        s_mn_iface->destroy(s_mn_model);
        s_mn_model = NULL;
    }

    if (s_afe) {
        afe_processor_destroy(s_afe);
        s_afe = NULL;
    }

    s_models = NULL;

    inmp441_deinit();

    ESP_LOGI(TAG, "Voice recognition deinitialized");
    return ESP_OK;
}
