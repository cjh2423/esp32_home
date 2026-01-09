#include "buzzer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";

esp_err_t buzzer_init(uint8_t gpio_num)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        return ret;
    }
    
    gpio_set_level(gpio_num, 1); // 默认高电平（不响）
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d (Active Low)", gpio_num);
    return ESP_OK;
}

esp_err_t buzzer_beep(uint8_t gpio_num, uint32_t duration_ms)
{
    gpio_set_level(gpio_num, 0); // 拉低响
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(gpio_num, 1); // 拉高停
    return ESP_OK;
}

esp_err_t buzzer_alarm(uint8_t gpio_num, uint8_t times)
{
    for (uint8_t i = 0; i < times; i++) {
        gpio_set_level(gpio_num, 0); // 拉低响
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(gpio_num, 1); // 拉高停
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return ESP_OK;
}
