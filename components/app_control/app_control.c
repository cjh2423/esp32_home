#include "app_control.h"
#include "esp_log.h"
#include "config.h"
// 引入硬件驱动
#include "led.h"
#include "fan.h"
#include "motor.h"
#include "buzzer.h"
#include "mq2.h" // 需要用到 mq2_is_smoke_detected

static const char *TAG = "APP_CTRL";

esp_err_t app_control_init(void)
{
    // 这里可以放一些控制逻辑相关的初始化，如果硬件初始化已经在 main 做过，这里可以留空
    // 或者将硬件初始化也移到这里，实现彻底解耦。
    // 为了保持 main 的简洁，我们假设硬件初始化在 main 完成，或者这里只做逻辑状态复位。
    ESP_LOGI(TAG, "App Control Initialized");
    return ESP_OK;
}

void app_control_process(sensor_data_t *data)
{
    if (data == NULL) return;

    // 1. 烟雾报警逻辑（安全优先）
    uint8_t fan_speed = data->fan_speed;
    uint8_t fan_state = data->fan_state;
    if (mq2_is_smoke_detected(data->smoke, SMOKE_THRESHOLD)) {
        ESP_LOGE(TAG, "Smoke Detected! Alarm!");
        buzzer_beep(BUZZER_GPIO, 200);
        
        fan_speed = 255;
        fan_state = 1;
    } else {
        // 2. 无烟雾时的温度控制逻辑
        if (AUTO_FAN_ENABLE) {
            if (data->temperature > 35.0f) {
                fan_speed = 255;
                fan_state = 1;
            } else if (data->temperature > 32.0f) {
                fan_speed = 200;
                fan_state = 1;
            } else if (data->temperature > TEMP_HIGH_THRESHOLD) { // > 30
                fan_speed = 150;
                fan_state = 1;
            } else if (data->temperature < TEMP_LOW_THRESHOLD) {
                fan_speed = 0;
                fan_state = 0;
            } else {
                fan_speed = 0;
                fan_state = 0;
            }
        } else {
            if (fan_state == 0) {
                fan_speed = 0;
            }
        }
    }

    // 3. 湿度控制逻辑 (已移除加湿器)
    /*
    if (AUTO_HUMIDIFIER_ENABLE) {
        // ...
    }
    */

    // 4. 灯光控制逻辑
    uint8_t led_brightness = data->led_brightness;
    uint8_t led_state = data->led_state;
    if (AUTO_LIGHT_ENABLE) {
        if (data->light < LIGHT_LOW_THRESHOLD) {
            led_state = 1;
            led_brightness = 255;
        } else {
            led_state = 0;
            led_brightness = 0;
        }
    } else {
        if (led_state == 0) {
            led_brightness = 0;
        } else if (led_brightness == 0) {
            led_brightness = 255;
        }
    }

    // 5. 窗帘控制逻辑
    static uint8_t last_curtain_state = 0;
    if (data->curtain_state != last_curtain_state) {
        curtain_control(data->curtain_state);
        last_curtain_state = data->curtain_state;
    }

    // 6. 同步风扇速度与状态
    data->fan_speed = fan_speed;
    data->fan_state = fan_state;
    fan_set_speed(fan_speed);

    // 7. 同步LED亮度和状态
    // 如果状态是关，强制占空比0；如果是开，使用 brightness
    data->led_state = led_state;
    data->led_brightness = led_brightness;
    if (led_state == 0) {
        led_off(LED_PWM_CHANNEL);
    } else {
        led_set_brightness(LED_PWM_CHANNEL, led_brightness);
    }
}
