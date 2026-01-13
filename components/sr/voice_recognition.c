#include "voice_recognition.h"
#include "inmp441_driver.h"
#include "afe_processor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>

// ESP-SR 库头文件
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_vad.h"

static const char *TAG = "VR";

// 事件组标志位 (参考 xiaozhi-esp32)
#define VR_EVENT_RUNNING    (1 << 0)

// 语音命令拼音定义
static const char *s_commands[] = {
    "da kai deng guang",    // ID 1: 打开灯光
    "guan bi deng guang",   // ID 2: 关闭灯光
    "da kai feng shan",     // ID 3: 打开风扇
    "guan bi feng shan",    // ID 4: 关闭风扇
    "hong se",              // ID 5: RGB 红色
    "lv se",                // ID 6: RGB 绿色
    "lan se",               // ID 7: RGB 蓝色
    "guan bi cai deng",     // ID 8: RGB 关闭
};
#define NUM_COMMANDS (sizeof(s_commands) / sizeof(s_commands[0]))

// 全局变量
static vr_command_callback_t s_callback = NULL;
static vr_vad_callback_t s_vad_callback = NULL;
static vr_vad_state_t s_last_vad_state = VR_VAD_SILENCE;
static TaskHandle_t s_feed_task_handle = NULL;
static TaskHandle_t s_detect_task_handle = NULL;
static volatile bool s_task_running = false;
static EventGroupHandle_t s_event_group = NULL;

// ESP-SR 模型句柄
static srmodel_list_t *s_models = NULL;
static const esp_mn_iface_t *s_mn_iface = NULL;
static model_iface_data_t *s_mn_model = NULL;

// AFE 处理器 (现在使用 AFE_TYPE_SR，内置 WakeNet)
static afe_processor_handle_t s_afe = NULL;

// 状态管理
typedef enum {
    VR_STATE_WAITING_WAKE,
    VR_STATE_WAITING_COMMAND
} vr_state_t;

static volatile vr_state_t s_state = VR_STATE_WAITING_WAKE;
static int s_mn_chunk = 0;

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
        case 5: return VR_CMD_RGB_RED;
        case 6: return VR_CMD_RGB_GREEN;
        case 7: return VR_CMD_RGB_BLUE;
        case 8: return VR_CMD_RGB_OFF;
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

    // 创建 AFE 处理器 (AFE_TYPE_SR，内置 WakeNet - 参考 xiaozhi)
    afe_processor_config_t afe_cfg = AFE_PROCESSOR_CONFIG_DEFAULT();
    s_afe = afe_processor_create(&afe_cfg, s_models);
    if (s_afe == NULL) {
        ESP_LOGE(TAG, "Failed to create AFE processor");
        goto err_cleanup_models;
    }

    // 注意：WakeNet 现在由 AFE 内部处理，不再需要外部加载
    ESP_LOGI(TAG, "WakeNet integrated in AFE (AFE_TYPE_SR mode)");

    // 加载 MultiNet 模型 (命令词识别仍需外部处理)
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, SR_MULTINET_MODEL);
    if (mn_name == NULL) {
        ESP_LOGE(TAG, "Failed to find MultiNet model: %s", SR_MULTINET_MODEL);
        goto err_cleanup_afe;
    }

    s_mn_iface = esp_mn_handle_from_name(mn_name);
    if (s_mn_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get MultiNet interface");
        goto err_cleanup_afe;
    }

    s_mn_model = s_mn_iface->create(mn_name, 5000);
    if (s_mn_model == NULL) {
        ESP_LOGE(TAG, "Failed to create MultiNet model");
        goto err_cleanup_afe;
    }
    s_mn_chunk = s_mn_iface->get_samp_chunksize(s_mn_model);
    if (s_mn_chunk <= 0) {
        ESP_LOGE(TAG, "Invalid MultiNet chunk size: %d", s_mn_chunk);
        goto err_cleanup_mn;
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
err_cleanup_afe:
    if (s_afe) {
        afe_processor_destroy(s_afe);
        s_afe = NULL;
    }
err_cleanup_models:
    s_models = NULL;
    s_mn_chunk = 0;
    return ESP_FAIL;
}

/**
 * @brief Feed 任务 - 负责 I2S 读取和 AFE 输入 (参考 xiaozhi AudioInputTask)
 */
static void vr_feed_task(void *arg)
{
    size_t feed_chunksize = afe_processor_get_feed_chunksize(s_afe);

    ESP_LOGI(TAG, "Feed task started (chunksize: %u)", (unsigned)feed_chunksize);

    int32_t *i2s_buffer = (int32_t *)malloc(feed_chunksize * sizeof(int32_t));
    if (i2s_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate I2S buffer");
        goto task_exit;
    }

    int16_t *audio_buffer = (int16_t *)malloc(feed_chunksize * sizeof(int16_t));
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        free(i2s_buffer);
        goto task_exit;
    }

    while (s_task_running) {
        EventBits_t bits = xEventGroupWaitBits(s_event_group, VR_EVENT_RUNNING,
                                                pdFALSE, pdTRUE, pdMS_TO_TICKS(100));
        if (!(bits & VR_EVENT_RUNNING) || !s_task_running) {
            continue;
        }

        size_t bytes_read = 0;
        esp_err_t ret = inmp441_read(i2s_buffer, feed_chunksize * sizeof(int32_t),
                                      &bytes_read, 100);
        if (ret != ESP_OK || bytes_read != feed_chunksize * sizeof(int32_t)) {
            continue;
        }

        for (size_t i = 0; i < feed_chunksize; i++) {
            audio_buffer[i] = (int16_t)(i2s_buffer[i] >> 16);
        }

        afe_processor_feed(s_afe, audio_buffer);
    }

    free(audio_buffer);
    free(i2s_buffer);

task_exit:
    ESP_LOGI(TAG, "Feed task stopped");
    s_feed_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Detect 任务 - 负责 AFE Fetch 和模型检测 (参考 xiaozhi AudioDetectionTask)
 *
 * 关键改进：WakeNet 检测由 AFE 内部完成，通过 wakeup_state 获取结果
 */
static void vr_detect_task(void *arg)
{
    size_t fetch_chunksize = afe_processor_get_fetch_chunksize(s_afe);

    ESP_LOGI(TAG, "Detect task started (chunksize: %u)", (unsigned)fetch_chunksize);

    if (s_mn_chunk <= 0 || fetch_chunksize == 0) {
        ESP_LOGE(TAG, "Invalid chunk size");
        goto task_exit;
    }

    int16_t *mn_accum = (int16_t *)malloc((size_t)s_mn_chunk * sizeof(int16_t));
    if (mn_accum == NULL) {
        ESP_LOGE(TAG, "Failed to allocate MultiNet buffer");
        goto task_exit;
    }

    size_t mn_accum_len = 0;

    while (s_task_running) {
        EventBits_t bits = xEventGroupWaitBits(s_event_group, VR_EVENT_RUNNING,
                                                pdFALSE, pdTRUE, portMAX_DELAY);
        if (!(bits & VR_EVENT_RUNNING) || !s_task_running) {
            continue;
        }

        // 使用扩展接口获取唤醒状态 (参考 xiaozhi afe_wake_word.cc:130)
        afe_fetch_result_t result;
        esp_err_t ret = afe_processor_fetch_ex(s_afe, &result, portMAX_DELAY);

        if (!(xEventGroupGetBits(s_event_group) & VR_EVENT_RUNNING)) {
            continue;
        }

        if (ret != ESP_OK || result.data == NULL) {
            continue;
        }

        // VAD 状态变化通知 (用于 RGB LED 亮度指示)
        vr_vad_state_t current_vad = (result.vad_state == VAD_SPEECH) ? VR_VAD_SPEECH : VR_VAD_SILENCE;
        if (current_vad != s_last_vad_state) {
            s_last_vad_state = current_vad;
            if (s_vad_callback) {
                s_vad_callback(current_vad);
            }
        }

        // 状态机处理
        if (s_state == VR_STATE_WAITING_WAKE) {
            // 关键：唤醒检测由 AFE 内部完成 (参考 xiaozhi afe_wake_word.cc:138)
            if (result.wakeup_state == WAKENET_DETECTED) {
                ESP_LOGI(TAG, "Wake word detected! (by AFE internal WakeNet)");
                s_state = VR_STATE_WAITING_COMMAND;
                mn_accum_len = 0;
                s_mn_iface->clean(s_mn_model);

                if (s_callback) {
                    s_callback(VR_CMD_WAKE_UP);
                }
            }
        }
        else if (s_state == VR_STATE_WAITING_COMMAND) {
            // MultiNet 命令词识别 (仍需外部处理)
            size_t offset = 0;
            while (offset < fetch_chunksize) {
                size_t remaining = fetch_chunksize - offset;
                size_t need = (size_t)s_mn_chunk - mn_accum_len;
                size_t to_copy = remaining < need ? remaining : need;

                memcpy(mn_accum + mn_accum_len, result.data + offset, to_copy * sizeof(int16_t));
                mn_accum_len += to_copy;
                offset += to_copy;

                if (mn_accum_len < (size_t)s_mn_chunk) {
                    break;
                }

                esp_mn_state_t mn_state = s_mn_iface->detect(s_mn_model, mn_accum);
                mn_accum_len = 0;

                // 让出 CPU，避免看门狗超时
                taskYIELD();

                if (mn_state == ESP_MN_STATE_DETECTED) {
                    esp_mn_results_t *mn_result = s_mn_iface->get_results(s_mn_model);
                    if (mn_result && mn_result->num > 0) {
                        int cmd_id = mn_result->phrase_id[0];
                        ESP_LOGI(TAG, "Command detected: ID %d", cmd_id);

                        vr_command_t cmd = map_command_id(cmd_id);
                        if (s_callback && cmd != VR_CMD_UNKNOWN) {
                            s_callback(cmd);
                        }

                        s_mn_iface->clean(s_mn_model);
                    }
                    s_state = VR_STATE_WAITING_WAKE;
                    break;
                } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
                    ESP_LOGI(TAG, "Command timeout, back to wake mode");
                    s_state = VR_STATE_WAITING_WAKE;
                    s_mn_iface->clean(s_mn_model);
                    break;
                }
            }
        }
    }

    free(mn_accum);

task_exit:
    ESP_LOGI(TAG, "Detect task stopped");
    s_detect_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t vr_init(int sck_io, int ws_io, int sd_io, vr_command_callback_t callback)
{
    if (callback == NULL) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    s_callback = callback;

    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    esp_err_t ret = inmp441_init(sck_io, ws_io, sd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init INMP441");
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        return ret;
    }

    ret = init_sr_models();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SR models");
        inmp441_deinit();
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Voice recognition initialized (AFE_TYPE_SR mode)");
    return ESP_OK;
}

esp_err_t vr_start(void)
{
    if (s_feed_task_handle != NULL || s_detect_task_handle != NULL) {
        ESP_LOGW(TAG, "Tasks already running");
        return ESP_OK;
    }

    s_task_running = true;
    s_state = VR_STATE_WAITING_WAKE;

    // Feed 任务 (高优先级, CPU 0)
    BaseType_t ret = xTaskCreatePinnedToCore(
        vr_feed_task, "vr_feed", 4 * 1024, NULL, 5, &s_feed_task_handle, 0);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create feed task");
        s_task_running = false;
        return ESP_FAIL;
    }

    // Detect 任务 (低优先级, CPU 1)
    ret = xTaskCreatePinnedToCore(
        vr_detect_task, "vr_detect", 8 * 1024, NULL, 3, &s_detect_task_handle, 1);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create detect task");
        s_task_running = false;
        vTaskDelay(pdMS_TO_TICKS(100));
        return ESP_FAIL;
    }

    xEventGroupSetBits(s_event_group, VR_EVENT_RUNNING);

    ESP_LOGI(TAG, "Voice recognition started (dual-task, AFE_TYPE_SR)");
    return ESP_OK;
}

esp_err_t vr_stop(void)
{
    if (s_feed_task_handle == NULL && s_detect_task_handle == NULL) {
        return ESP_OK;
    }

    xEventGroupClearBits(s_event_group, VR_EVENT_RUNNING);
    s_task_running = false;

    if (s_afe) {
        afe_processor_reset(s_afe);
    }

    for (int i = 0; i < 100 && (s_feed_task_handle != NULL || s_detect_task_handle != NULL); i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    s_state = VR_STATE_WAITING_WAKE;

    if (s_mn_iface && s_mn_model) {
        s_mn_iface->clean(s_mn_model);
    }

    ESP_LOGI(TAG, "Voice recognition stopped");
    return ESP_OK;
}

esp_err_t vr_deinit(void)
{
    vr_stop();

    if (s_mn_iface && s_mn_model) {
        s_mn_iface->destroy(s_mn_model);
        s_mn_model = NULL;
    }

    if (s_afe) {
        afe_processor_destroy(s_afe);
        s_afe = NULL;
    }

    if (s_event_group) {
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
    }

    s_models = NULL;
    s_mn_chunk = 0;

    inmp441_deinit();

    ESP_LOGI(TAG, "Voice recognition deinitialized");
    return ESP_OK;
}

void vr_set_vad_callback(vr_vad_callback_t callback)
{
    s_vad_callback = callback;
}
