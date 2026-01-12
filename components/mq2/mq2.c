#include "mq2.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "MQ2";
static adc_oneshot_unit_handle_t adc1_handle = NULL;

esp_err_t mq2_init(adc1_channel_t adc_channel)
{
    // 配置ADC单元
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed");
        return ret;
    }
    
    // 配置ADC通道
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // 0-3.3V范围 (ESP-IDF 5.x)
    };
    
    ret = adc_oneshot_config_channel(adc1_handle, adc_channel, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed");
        return ret;
    }
    
    ESP_LOGI(TAG, "MQ-2 initialized on ADC1 channel %d", adc_channel);
    return ESP_OK;
}

esp_err_t mq2_read(adc1_channel_t adc_channel, uint32_t *value)
{
    if (value == NULL || adc1_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int raw_value;
    esp_err_t ret = adc_oneshot_read(adc1_handle, adc_channel, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed");
        return ret;
    }
    
    *value = (uint32_t)raw_value;
    // ESP_LOGI(TAG, "MQ-2 ADC value: %lu", *value);
    
    return ESP_OK;
}

uint8_t mq2_is_smoke_detected(uint32_t value, uint32_t threshold)
{
    return (value > threshold) ? 1 : 0;
}
