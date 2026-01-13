#include "voice_recognition.h"
#include "inmp441_driver.h"
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

// ESP-SR 模型句柄
static const esp_wn_iface_t *s_wn_iface = NULL;
static model_iface_data_t *s_wn_model = NULL;
static const esp_mn_iface_t *s_mn_iface = NULL;
static model_iface_data_t *s_mn_model = NULL;

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
    srmodel_list_t *models = esp_srmodel_init("model");
    if (models == NULL) {
        ESP_LOGE(TAG, "Failed to init SR model list");
        return ESP_FAIL;
    }

    // 加载 WakeNet 模型
    char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, SR_WAKENET_MODEL);
    if (wn_name == NULL) {
        ESP_LOGE(TAG, "Failed to find WakeNet model: %s", SR_WAKENET_MODEL);
        return ESP_FAIL;
    }

    s_wn_iface = esp_wn_handle_from_name(wn_name);
    if (s_wn_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get WakeNet interface");
        return ESP_FAIL;
    }

    s_wn_model = s_wn_iface->create(wn_name, SR_WAKENET_MODE);
    if (s_wn_model == NULL) {
        ESP_LOGE(TAG, "Failed to create WakeNet model");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WakeNet ready (SR: %d Hz, Chunk: %d)",
             s_wn_iface->get_samp_rate(s_wn_model),
             s_wn_iface->get_samp_chunksize(s_wn_model));

    // 加载 MultiNet 模型
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, SR_MULTINET_MODEL);
    if (mn_name == NULL) {
        ESP_LOGE(TAG, "Failed to find MultiNet model: %s", SR_MULTINET_MODEL);
        return ESP_FAIL;
    }

    s_mn_iface = esp_mn_handle_from_name(mn_name);
    if (s_mn_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get MultiNet interface");
        return ESP_FAIL;
    }

    s_mn_model = s_mn_iface->create(mn_name, 5000);
    if (s_mn_model == NULL) {
        ESP_LOGE(TAG, "Failed to create MultiNet model");
        return ESP_FAIL;
    }

    // 注册自定义命令
    esp_mn_commands_clear();
    for (int i = 0; i < NUM_COMMANDS; i++) {
        esp_mn_commands_add(i + 1, (char *)s_commands[i]);
    }
    
    esp_mn_error_t *err = esp_mn_commands_update();
    if (err) {
        ESP_LOGE(TAG, "Failed to update commands");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MultiNet ready (%d commands)", NUM_COMMANDS);
    return ESP_OK;
}

/**
 * @brief 语音识别主任务
 */
static void vr_task(void *arg)
{
    int chunk_size = s_wn_iface->get_samp_chunksize(s_wn_model);
    int chunk_bytes = chunk_size * sizeof(int16_t);

    // 分配音频缓冲区
    int16_t *audio_buffer = (int16_t *)malloc(chunk_bytes);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }

    // 由于 INMP441 输出 32 位数据，需要临时缓冲区
    int32_t *i2s_buffer = (int32_t *)malloc(chunk_size * sizeof(int32_t));
    if (i2s_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate I2S buffer");
        free(audio_buffer);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Task started");

    while (1) {
        size_t bytes_read = 0;
        
        // 从 INMP441 读取数据
        esp_err_t ret = inmp441_read(i2s_buffer, chunk_size * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        if (ret != ESP_OK || bytes_read != chunk_size * sizeof(int32_t)) {
            ESP_LOGW(TAG, "I2S read failed or incomplete");
            continue;
        }

        // 转换 32 位数据到 16 位（INMP441 24 位数据存储在高 24 位）
        for (int i = 0; i < chunk_size; i++) {
            audio_buffer[i] = (int16_t)(i2s_buffer[i] >> 16);
        }

        // 根据状态处理音频
        if (s_state == VR_STATE_WAITING_WAKE) {
            wakenet_state_t wn_state = s_wn_iface->detect(s_wn_model, audio_buffer);
            
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
            esp_mn_state_t mn_state = s_mn_iface->detect(s_mn_model, audio_buffer);
            
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
            } 
            else if (mn_state == ESP_MN_STATE_TIMEOUT) {
                ESP_LOGI(TAG, "Command timeout, back to wake mode");
                s_state = VR_STATE_WAITING_WAKE;
            }
        }
    }

    free(audio_buffer);
    free(i2s_buffer);
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
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t vr_stop(void)
{
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
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
    
    inmp441_deinit();
    
    ESP_LOGI(TAG, "Voice recognition deinitialized");
    return ESP_OK;
}
