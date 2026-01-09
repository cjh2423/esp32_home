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

    // 1. 烟雾报警逻辑
    if (mq2_is_smoke_detected(data->smoke, SMOKE_THRESHOLD)) {
        ESP_LOGE(TAG, "Smoke Detected! Alarm!");
        buzzer_beep(BUZZER_GPIO, 200);
        
        if (AUTO_FAN_ENABLE) {
            fan_set_speed(255); // 全速
            data->fan_state = 1;
            data->fan_speed = 255;
        }
    } else {
        // 2. 无烟雾时的温度控制逻辑
        if (AUTO_FAN_ENABLE) {
             if (data->temperature > 35.0) {
                 fan_set_speed(255);
                 data->fan_state = 1;
                 data->fan_speed = 255;
             } else if (data->temperature > 32.0) {
                 fan_set_speed(200);
                 data->fan_state = 1;
                 data->fan_speed = 200;
             } else if (data->temperature > TEMP_HIGH_THRESHOLD) { // > 30
                 fan_set_speed(150);
                 data->fan_state = 1;
                 data->fan_speed = 150;
             } else if (data->temperature < TEMP_LOW_THRESHOLD) {
                 fan_set_speed(0);
                 data->fan_state = 0;
                 data->fan_speed = 0;
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
    if (AUTO_LIGHT_ENABLE) {
        if (data->light < LIGHT_LOW_THRESHOLD) {
            if (data->led_state == 0) {
                led_on(LED_PWM_CHANNEL);
                data->led_state = 1;
                data->led_brightness = 255;
            }
        } else {
            if (data->led_state == 1) {
                led_off(LED_PWM_CHANNEL);
                data->led_state = 0;
                data->led_brightness = 0;
            }
        }
    }

    // 5. 窗帘控制逻辑
    static uint8_t last_curtain_state = 0;
    if (data->curtain_state != last_curtain_state) {
        curtain_control(data->curtain_state);
        last_curtain_state = data->curtain_state;
    }

    // 6. 同步Web设置的风扇速度 (无论是自动还是手动修改)
    fan_set_speed(data->fan_speed);
    
    // 7. 同步LED亮度和状态
    // 如果状态是关，强制占空比0；如果是开，使用 brightness
    if (data->led_state == 0) {
        led_off(LED_PWM_CHANNEL);
    } else {
        // 使用 led 驱动中的设置占空比函数，这里假设 led_on 其实就是设占空比
        // 但 led.c 里 led_on 是直接设为 255 的吗？
        // 我们最好用 led_set_brightness (如果存在)
        // 检查 led.h，如果没有 led_set_brightness，就用 ledc_set_duty
        led_set_brightness(LED_PWM_CHANNEL, data->led_brightness);
    }
}
