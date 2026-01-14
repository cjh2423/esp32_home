/**
 * @file servo_driver.c
 * @brief 舵机驱动封装 - 使用本地修改的 iot_servo 驱动
 *
 * 基于 espressif/servo 组件，已修改时钟配置 (LEDC_APB_CLK) 避免冲突
 */

#include "servo_driver.h"
#include "iot_servo.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "SERVO";

// 舵机配置参数
#define SERVO_MIN_WIDTH_US  500   // 0度脉宽
#define SERVO_MAX_WIDTH_US  2500  // 180度脉宽
#define SERVO_MAX_ANGLE     180
#define SERVO_FREQ          50    // 50Hz

static bool s_initialized = false;

esp_err_t motor_init(uint8_t servo_gpio)
{
    servo_config_t servo_cfg = {
        .max_angle = SERVO_MAX_ANGLE,
        .min_width_us = SERVO_MIN_WIDTH_US,
        .max_width_us = SERVO_MAX_WIDTH_US,
        .freq = SERVO_FREQ,
        .timer_number = LEDC_TIMER_3,  // 使用 Timer 3，避免与 LED(0)/Fan(2) 冲突
        .channels = {
            .servo_pin = {servo_gpio, -1, -1, -1, -1, -1, -1, -1},
            .ch = {LEDC_CHANNEL_2, -1, -1, -1, -1, -1, -1, -1},
        },
        .channel_number = 1,
    };

    esp_err_t ret = iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Servo init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始位置设置为 0 度
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);

    s_initialized = true;
    ESP_LOGI(TAG, "Servo initialized on GPIO %d (TIMER_3, APB_CLK)", servo_gpio);
    return ESP_OK;
}

esp_err_t servo_set_angle(float angle)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    return iot_servo_write_angle(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, angle);
}

esp_err_t curtain_control(uint8_t open)
{
    // open = 1 -> 打开窗帘 -> 舵机转到 180度
    // open = 0 -> 关闭窗帘 -> 舵机转到 0度
    float angle = open ? 180.0f : 0.0f;

    esp_err_t ret = servo_set_angle(angle);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Curtain set to %s (Angle: %.0f)",
                 open ? "OPEN" : "CLOSED", angle);
    }
    return ret;
}

void motor_deinit(void)
{
    if (s_initialized) {
        iot_servo_deinit(LEDC_LOW_SPEED_MODE);
        s_initialized = false;
        ESP_LOGI(TAG, "Servo deinitialized");
    }
}
