#include "led.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "LED";

#define LED_PWM_FREQ 5000
#define LED_PWM_RESOLUTION LEDC_TIMER_8_BIT

esp_err_t led_init(uint8_t gpio_num, uint8_t channel)
{
    // 配置定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LED_PWM_RESOLUTION,
        .freq_hz = LED_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed");
        return ret;
    }
    
    // 配置通道
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio_num,
        .duty = 0,
        .hpoint = 0
    };
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed");
        return ret;
    }
    
    ESP_LOGI(TAG, "LED initialized on GPIO %d, channel %d", gpio_num, channel);
    return ESP_OK;
}

esp_err_t led_set_brightness(uint8_t channel, uint8_t brightness)
{
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, brightness);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set duty failed");
        return ret;
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Update duty failed");
        return ret;
    }
    
    // ESP_LOGI(TAG, "LED brightness set to %d", brightness);
    return ESP_OK;
}

esp_err_t led_on(uint8_t channel)
{
    return led_set_brightness(channel, 255);
}

esp_err_t led_off(uint8_t channel)
{
    return led_set_brightness(channel, 0);
}
