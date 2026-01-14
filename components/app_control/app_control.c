#include "app_control.h"
#include "app_state.h"
#include "voice_recognition.h"
#include "esp_log.h"
#include "config.h"
// 引入硬件驱动
#include "led.h"
#include "fan.h"
#include "servo_driver.h"  // 使用 espressif/servo 官方组件
#include "buzzer.h"
#include "mq2.h" // 需要用到 mq2_is_smoke_detected
#include "rgb_led.h"

static const char *TAG = "APP_CTRL";

// RGB LED 亮度配置
#define RGB_BRIGHTNESS_BASE   10   // 基础亮度 (静音时) - 降低以节能
#define RGB_BRIGHTNESS_SPEECH 60   // 语音时亮度

// 当前 RGB 颜色状态 (RGB 始终保持亮着)
static rgb_color_t s_current_rgb_color = RGB_COLOR_GREEN;
static rgb_color_t s_saved_rgb_color = RGB_COLOR_GREEN;  // 唤醒前保存的颜色
static uint8_t s_last_brightness = 0;  // 缓存上次亮度，避免冗余硬件操作

// 滞回状态记录
static struct {
    bool fan_on;       // 风扇当前是否开启
    bool led_on;       // LED当前是否开启
} hysteresis_state = {false, false};

esp_err_t app_control_init(void)
{
    hysteresis_state.fan_on = false;
    hysteresis_state.led_on = false;

    // 初始化 RGB 为绿色常亮 (基础亮度)
    s_last_brightness = RGB_BRIGHTNESS_BASE;
    rgb_led_set_brightness(RGB_BRIGHTNESS_BASE);
    rgb_led_set_color(s_current_rgb_color);

    ESP_LOGI(TAG, "App Control Initialized (RGB Green)");
    return ESP_OK;
}

void app_control_process(sensor_data_t *data)
{
    if (data == NULL) return;

    // 检查控制模式 - 手动模式下跳过自动控制逻辑（烟雾报警除外）
    bool is_auto_mode = (data->control_mode == CONTROL_MODE_AUTO);

    // 1. 烟雾报警逻辑（安全优先 - 任何模式下都执行）
    uint8_t fan_speed = data->fan_speed;
    uint8_t fan_state = data->fan_state;
    if (mq2_is_smoke_detected(data->smoke, SMOKE_THRESHOLD)) {
        ESP_LOGE(TAG, "Smoke Detected! Alarm!");
        buzzer_beep(BUZZER_GPIO, BUZZER_BEEP_DURATION_MS);

        fan_speed = FAN_SPEED_HIGH;
        fan_state = 1;
        hysteresis_state.fan_on = true;
    } else if (is_auto_mode) {
        // 2. 自动模式：温度控制逻辑（带滞回）
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
        }
    }
    // 手动模式：保持用户设置的风扇状态，不自动调整

    // 3. 灯光控制逻辑（带滞回）- 仅自动模式
    uint8_t led_brightness = data->led_brightness;
    uint8_t led_state = data->led_state;
    if (is_auto_mode && AUTO_LIGHT_ENABLE) {
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
    }
    // 手动模式：保持用户设置的LED状态，不自动调整

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

void app_control_set_mode(sensor_data_t *data, control_mode_t mode)
{
    if (data == NULL) return;

    data->control_mode = mode;
    if (mode == CONTROL_MODE_AUTO) {
        // 切回自动模式时，同步滞回状态与当前设备状态
        hysteresis_state.fan_on = (data->fan_state != 0);
        hysteresis_state.led_on = (data->led_state != 0);
    }
}

void app_control_handle_voice_command(vr_command_t command)
{
    // 唤醒命令
    if (command == VR_CMD_WAKE_UP) {
        ESP_LOGI(TAG, "Voice: Wake up detected");
        buzzer_beep(BUZZER_GPIO, 100);

        // 保存当前颜色并切换到橙色
        s_saved_rgb_color = s_current_rgb_color;
        s_current_rgb_color = RGB_COLOR_ORANGE;
        rgb_led_set_color(RGB_COLOR_ORANGE);
        return;
    }

    // 超时命令
    if (command == VR_CMD_TIMEOUT) {
        ESP_LOGI(TAG, "Voice: Timeout, exit listening mode");

        // 恢复唤醒前的颜色
        s_current_rgb_color = s_saved_rgb_color;
        rgb_led_set_color(s_current_rgb_color);
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

        case VR_CMD_RGB_RED:
            ESP_LOGI(TAG, "Voice: RGB Red");
            s_current_rgb_color = RGB_COLOR_RED;
            s_saved_rgb_color = RGB_COLOR_RED;  // 保留用户选择
            rgb_led_set_color(RGB_COLOR_RED);
            break;

        case VR_CMD_RGB_GREEN:
            ESP_LOGI(TAG, "Voice: RGB Green");
            s_current_rgb_color = RGB_COLOR_GREEN;
            s_saved_rgb_color = RGB_COLOR_GREEN;  // 保留用户选择
            rgb_led_set_color(RGB_COLOR_GREEN);
            break;

        case VR_CMD_RGB_BLUE:
            ESP_LOGI(TAG, "Voice: RGB Blue");
            s_current_rgb_color = RGB_COLOR_BLUE;
            s_saved_rgb_color = RGB_COLOR_BLUE;  // 保留用户选择
            rgb_led_set_color(RGB_COLOR_BLUE);
            break;

        case VR_CMD_CURTAIN_OPEN:
            ESP_LOGI(TAG, "Voice: Open curtain");
            data->curtain_state = 1;
            curtain_control(1);
            break;

        case VR_CMD_CURTAIN_CLOSE:
            ESP_LOGI(TAG, "Voice: Close curtain");
            data->curtain_state = 0;
            curtain_control(0);
            break;

        case VR_CMD_MODE_AUTO:
            ESP_LOGI(TAG, "Voice: Switch to AUTO mode");
            app_control_set_mode(data, CONTROL_MODE_AUTO);
            buzzer_beep(BUZZER_GPIO, 50);  // 短提示音
            break;

        case VR_CMD_MODE_MANUAL:
            ESP_LOGI(TAG, "Voice: Switch to MANUAL mode");
            app_control_set_mode(data, CONTROL_MODE_MANUAL);
            buzzer_beep(BUZZER_GPIO, 100);  // 长提示音区分
            break;

        default:
            ESP_LOGW(TAG, "Unknown voice command: %d", command);
            break;
    }

    app_state_unlock();
}

void app_control_handle_vad_state(vr_vad_state_t state)
{
    // VAD 状态控制亮度：语音时高亮，静音时低亮
    uint8_t target_brightness = (state == VR_VAD_SPEECH) ? RGB_BRIGHTNESS_SPEECH : RGB_BRIGHTNESS_BASE;

    // 仅在亮度变化时才操作硬件，避免冗余 I2C/GPIO 调用
    if (s_last_brightness != target_brightness) {
        s_last_brightness = target_brightness;
        rgb_led_set_brightness(target_brightness);
        rgb_led_set_color(s_current_rgb_color);
    }
}
