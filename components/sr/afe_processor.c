/**
 * @file afe_processor.c
 * @brief ESP AFE (Audio Front-End) 音频处理器实现
 *
 * 参考 xiaozhi-esp32 afe_audio_processor.cc 实现
 */

#include "afe_processor.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "AFE";

/**
 * @brief AFE 处理器内部结构
 */
struct afe_processor {
    const esp_afe_sr_iface_t *afe_iface;
    esp_afe_sr_data_t *afe_data;
    srmodel_list_t *models;
    bool models_owned;  // 是否拥有 models 的所有权
    int feed_chunksize;
    int fetch_chunksize;
};

afe_processor_handle_t afe_processor_create(const afe_processor_config_t *config, srmodel_list_t *models)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }

    afe_processor_handle_t handle = calloc(1, sizeof(struct afe_processor));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate handle");
        return NULL;
    }

    // 加载或使用已有模型列表
    if (models == NULL) {
        handle->models = esp_srmodel_init("model");
        handle->models_owned = true;
        if (handle->models == NULL) {
            ESP_LOGE(TAG, "Failed to init SR models");
            free(handle);
            return NULL;
        }
    } else {
        handle->models = models;
        handle->models_owned = false;
    }

    // 查找 NS 和 VAD 模型
    char *ns_model_name = NULL;
    char *vad_model_name = NULL;

    if (config->enable_ns) {
        ns_model_name = esp_srmodel_filter(handle->models, ESP_NSNET_PREFIX, NULL);
        if (ns_model_name == NULL) {
            ESP_LOGW(TAG, "NS model not found, disabling NS");
        }
    }

    if (config->enable_vad) {
        vad_model_name = esp_srmodel_filter(handle->models, ESP_VADN_PREFIX, NULL);
        if (vad_model_name == NULL) {
            ESP_LOGW(TAG, "VAD model not found, using default VAD");
        }
    }

    // 配置 AFE - 单麦克风模式 "M"
    afe_config_t *afe_config = afe_config_init("M", NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    if (afe_config == NULL) {
        ESP_LOGE(TAG, "Failed to init AFE config");
        goto err_cleanup;
    }

    // AEC 配置 - 无扬声器，禁用 AEC
    afe_config->aec_init = false;

    // VAD 配置
    afe_config->vad_init = config->enable_vad;
    afe_config->vad_mode = VAD_MODE_0;
    afe_config->vad_min_noise_ms = 100;
    if (vad_model_name != NULL) {
        afe_config->vad_model_name = vad_model_name;
    }

    // NS 配置
    if (ns_model_name != NULL) {
        afe_config->ns_init = true;
        afe_config->ns_model_name = ns_model_name;
        afe_config->afe_ns_mode = AFE_NS_MODE_NET;
        ESP_LOGI(TAG, "NS enabled with model: %s", ns_model_name);
    } else {
        afe_config->ns_init = false;
    }

    // AGC 配置
    afe_config->agc_init = config->enable_agc;

    // 内存分配模式
    if (config->use_psram) {
        afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    } else {
        afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_INTERNAL;
    }

    // 创建 AFE 实例
    handle->afe_iface = esp_afe_handle_from_config(afe_config);
    if (handle->afe_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get AFE interface");
        goto err_cleanup;
    }

    handle->afe_data = handle->afe_iface->create_from_config(afe_config);
    if (handle->afe_data == NULL) {
        ESP_LOGE(TAG, "Failed to create AFE data");
        goto err_cleanup;
    }

    // 获取 chunk 大小
    handle->feed_chunksize = handle->afe_iface->get_feed_chunksize(handle->afe_data);
    handle->fetch_chunksize = handle->afe_iface->get_fetch_chunksize(handle->afe_data);

    ESP_LOGI(TAG, "AFE created (feed: %d, fetch: %d, NS: %s, VAD: %s)",
             handle->feed_chunksize, handle->fetch_chunksize,
             ns_model_name ? "ON" : "OFF",
             config->enable_vad ? "ON" : "OFF");

    return handle;

err_cleanup:
    if (handle->models_owned && handle->models) {
        // esp_srmodel_deinit 不存在，模型列表由 SDK 管理
    }
    free(handle);
    return NULL;
}

void afe_processor_destroy(afe_processor_handle_t handle)
{
    if (handle == NULL) return;

    if (handle->afe_iface && handle->afe_data) {
        handle->afe_iface->destroy(handle->afe_data);
    }

    free(handle);
    ESP_LOGI(TAG, "AFE destroyed");
}

size_t afe_processor_get_feed_chunksize(afe_processor_handle_t handle)
{
    if (handle == NULL) return 0;
    return handle->feed_chunksize;
}

size_t afe_processor_get_fetch_chunksize(afe_processor_handle_t handle)
{
    if (handle == NULL) return 0;
    return handle->fetch_chunksize;
}

esp_err_t afe_processor_feed(afe_processor_handle_t handle, int16_t *data)
{
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->afe_iface && handle->afe_data) {
        handle->afe_iface->feed(handle->afe_data, data);
        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

esp_err_t afe_processor_fetch(afe_processor_handle_t handle, int16_t **out_data,
                               afe_vad_state_t *out_vad, uint32_t timeout_ms)
{
    if (handle == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->afe_iface == NULL || handle->afe_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    afe_fetch_result_t *res = handle->afe_iface->fetch_with_delay(
        handle->afe_data, pdMS_TO_TICKS(timeout_ms));

    if (res == NULL) {
        return ESP_ERR_TIMEOUT;
    }

    if (res->ret_value == ESP_FAIL) {
        return ESP_FAIL;
    }

    *out_data = res->data;

    if (out_vad) {
        *out_vad = (res->vad_state == VAD_SPEECH) ? AFE_VAD_SPEECH : AFE_VAD_SILENCE;
    }

    return ESP_OK;
}

void afe_processor_reset(afe_processor_handle_t handle)
{
    if (handle && handle->afe_iface && handle->afe_data) {
        handle->afe_iface->reset_buffer(handle->afe_data);
    }
}

int afe_processor_get_sample_rate(afe_processor_handle_t handle)
{
    // AFE 固定使用 16kHz
    return 16000;
}
