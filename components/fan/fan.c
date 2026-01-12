#include "fan.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "FAN_CTRL";
static uint8_t g_fan_channel;

esp_err_t fan_init(uint8_t fan_gpio, uint8_t fan_channel)
{
    g_fan_channel = fan_channel;
    
    // 1. 定时器配置 (FAN必须使用和LED不同的Timer吗? 不是必须，如果频率一样可以共用)
    // 但Config里分别定义了FREQ。
    // LED用TIMER_0, 舵机用TIMER_1
    // 风扇可以用 TIMER_2
    
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_2,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 25000,  // 25kHz 通常适合4线PWM风扇
        .clk_cfg          = LEDC_AUTO_CLK
    };
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        ESP_LOGE(TAG, "Fan Timer config failed");
        return ESP_FAIL;
    }

    // 2. 通道配置
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = fan_channel,
        .timer_sel      = LEDC_TIMER_2,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = fan_gpio,
        .duty           = 255, // 初始关闭 (低电平触发: 0=全速, 255=停？待确认)
                               // Config里说0=全速，255=停？
                               // 我们先按之前的逻辑：set_speed(0) -> duty=255(High) -> Fan Off
        .hpoint         = 0
    };
    if (ledc_channel_config(&ledc_channel) != ESP_OK) {
        ESP_LOGE(TAG, "Fan Channel config failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Fan (PWM Ch%d) initialized", fan_channel);
    return ESP_OK;
}

esp_err_t fan_set_speed(uint8_t speed)
{
    // 假设风扇是低电平触发/控制 (PWM值越小转越快？或者根据MOS管电路)
    // 通常 PWM 风扇：High Duty = High Speed
    // 但如果是用 N-MOS 控制负极的直流电机：High Duty (Gate High) = Conduction = Motor Run
    // 之前的代码逻辑是: duty = 255 - speed; (speed 0 -> duty 255 -> 100% High)
    // 之前的注释说: "0=stop, 255=full speed" -> 但逻辑是 "duty = 255 - speed"
    // 这意味着 speed=0 -> duty=255 (全高) -> 风扇停？
    // 那说明这是 PNP 控制或者 低电平有效的 PWM 输入？
    // 我们保持原有的反转逻辑，假设这适配您的硬件。
    
    uint32_t duty = 255 - speed;

    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, g_fan_channel, duty);
    if (ret != ESP_OK) return ret;
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, g_fan_channel);
    
    // ESP_LOGI(TAG, "Fan speed set to %d (Duty: %lu)", speed, duty);
    return ret;
}

// 移除 humidifier_control
