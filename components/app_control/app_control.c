#include "app_control.h"
#include "app_state.h"
#include "voice_recognition.h"
#include "esp_log.h"
#include "config.h"
// 引入硬件驱动
#include "led.h"
#include "fan.h"
#include "motor.h"
#include "buzzer.h"
#include "mq2.h" // 需要用到 mq2_is_smoke_detected

static const char *TAG = "APP_CTRL";

// 滞回状态记录
static struct {
    bool fan_on;       // 风扇当前是否开启
    bool led_on;       // LED当前是否开启
} hysteresis_state = {false, false};

esp_err_t app_control_init(void)
{
    hysteresis_state.fan_on = false;
    hysteresis_state.led_on = false;
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
        buzzer_beep(BUZZER_GPIO, BUZZER_BEEP_DURATION_MS);

        fan_speed = FAN_SPEED_HIGH;
        fan_state = 1;
        hysteresis_state.fan_on = true;
    } else {
        // 2. 无烟雾时的温度控制逻辑（带滞回）
        if (AUTO_FAN_ENABLE) {
            float temp = data->temperature;

            // 滞回逻辑：开启阈值和关闭阈值不同
            if (hysteresis_state.fan_on) {
                // 当前开启状态，需要低于 (阈值 - 滞回值) 才关闭
                if (temp < (TEMP_HIGH_THRESHOLD - TEMP_HYSTERESIS)) {
                    fan_speed = FAN_SPEED_OFF;
                    fan_state = 0;
                    hysteresis_state.fan_on = false;
                } else if (temp > TEMP_CRITICAL_THRESHOLD) {
                    fan_speed = FAN_SPEED_HIGH;
                    fan_state = 1;
                } else if (temp > TEMP_MEDIUM_THRESHOLD) {
                    fan_speed = FAN_SPEED_MEDIUM;
                    fan_state = 1;
                } else {
                    fan_speed = FAN_SPEED_LOW;
                    fan_state = 1;
                }
            } else {
                // 当前关闭状态，需要高于阈值才开启
                if (temp > TEMP_CRITICAL_THRESHOLD) {
                    fan_speed = FAN_SPEED_HIGH;
                    fan_state = 1;
                    hysteresis_state.fan_on = true;
                } else if (temp > TEMP_MEDIUM_THRESHOLD) {
                    fan_speed = FAN_SPEED_MEDIUM;
                    fan_state = 1;
                    hysteresis_state.fan_on = true;
                } else if (temp > TEMP_HIGH_THRESHOLD) {
                    fan_speed = FAN_SPEED_LOW;
                    fan_state = 1;
                    hysteresis_state.fan_on = true;
                } else {
                    fan_speed = FAN_SPEED_OFF;
                    fan_state = 0;
                }
            }
        } else {
            if (fan_state == 0) {
                fan_speed = FAN_SPEED_OFF;
            }
        }
    }

    // 3. 灯光控制逻辑（带滞回）
    uint8_t led_brightness = data->led_brightness;
    uint8_t led_state = data->led_state;
    if (AUTO_LIGHT_ENABLE) {
        float light = data->light;

        // 滞回逻辑
        if (hysteresis_state.led_on) {
            // 当前开启状态，需要高于 (阈值 + 滞回值) 才关闭
            if (light > (LIGHT_LOW_THRESHOLD + LIGHT_HYSTERESIS)) {
                led_state = 0;
                led_brightness = LED_BRIGHTNESS_OFF;
                hysteresis_state.led_on = false;
            }
        } else {
            // 当前关闭状态，需要低于阈值才开启
            if (light < LIGHT_LOW_THRESHOLD) {
                led_state = 1;
                led_brightness = LED_BRIGHTNESS_MAX;
                hysteresis_state.led_on = true;
            }
        }
    } else {
        if (led_state == 0) {
            led_brightness = LED_BRIGHTNESS_OFF;
        } else if (led_brightness == 0) {
            led_brightness = LED_BRIGHTNESS_MAX;
        }
    }

    // 4. 窗帘控制逻辑
    static uint8_t last_curtain_state = 0;
    if (data->curtain_state != last_curtain_state) {
        curtain_control(data->curtain_state);
        last_curtain_state = data->curtain_state;
    }

    // 5. 同步风扇速度与状态
    data->fan_speed = fan_speed;
    data->fan_state = fan_state;
    fan_set_speed(fan_speed);

    // 6. 同步LED亮度和状态
    data->led_state = led_state;
    data->led_brightness = led_brightness;
    if (led_state == 0) {
        led_off(LED_PWM_CHANNEL);
    } else {
        led_set_brightness(LED_PWM_CHANNEL, led_brightness);
    }
}

void app_control_handle_voice_command(vr_command_t command)
{
    // 唤醒命令：蜂鸣器提示（不需要锁，避免阻塞临界区）
    if (command == VR_CMD_WAKE_UP) {
        ESP_LOGI(TAG, "Voice: Wake up detected");
        buzzer_beep(BUZZER_GPIO, 100);
        return;
    }

    sensor_data_t *data = app_state_get();
    if (data == NULL) return;

    // 检查锁返回值，失败则不操作
    if (app_state_lock() != ESP_OK) {
        ESP_LOGW(TAG, "Voice command dropped: lock failed");
        return;
    }

    switch (command) {
        case VR_CMD_LIGHT_ON:
            ESP_LOGI(TAG, "Voice: Turn on light");
            data->led_state = 1;
            data->led_brightness = LED_BRIGHTNESS_MAX;
            led_set_brightness(LED_PWM_CHANNEL, LED_BRIGHTNESS_MAX);
            hysteresis_state.led_on = true;
            break;

        case VR_CMD_LIGHT_OFF:
            ESP_LOGI(TAG, "Voice: Turn off light");
            data->led_state = 0;
            data->led_brightness = LED_BRIGHTNESS_OFF;
            led_off(LED_PWM_CHANNEL);
            hysteresis_state.led_on = false;
            break;

        case VR_CMD_FAN_ON:
            ESP_LOGI(TAG, "Voice: Turn on fan");
            data->fan_state = 1;
            data->fan_speed = FAN_SPEED_MEDIUM;
            fan_set_speed(FAN_SPEED_MEDIUM);
            hysteresis_state.fan_on = true;
            break;

        case VR_CMD_FAN_OFF:
            ESP_LOGI(TAG, "Voice: Turn off fan");
            data->fan_state = 0;
            data->fan_speed = FAN_SPEED_OFF;
            fan_set_speed(FAN_SPEED_OFF);
            hysteresis_state.fan_on = false;
            break;

        default:
            ESP_LOGW(TAG, "Unknown voice command: %d", command);
            break;
    }

    app_state_unlock();
}

