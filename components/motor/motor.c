#include "motor.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "SERVO";

// 舵机角度对应的脉宽 (0.5ms - 2.5ms)
// 50Hz 周期 20ms
// 0度 -> 0.5ms -> (0.5/20) * 65535 (如有16位分辨率) -> 2.5% duty
// 180度 -> 2.5ms -> 12.5% duty

#define SERVO_MIN_PULSEWIDTH_US 500  // 0度
#define SERVO_MAX_PULSEWIDTH_US 2500 // 180度
#define SERVO_MAX_DEGREE        180

static uint32_t calculate_duty(int angle)
{
    if (angle > SERVO_MAX_DEGREE) angle = SERVO_MAX_DEGREE;
    if (angle < 0) angle = 0;

    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH_US + 
                           (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) / SERVO_MAX_DEGREE);
    
    // 占空比计算 (分辨率13位 = 8191)
    // Duty = (PulseWidth / 20000us) * 8191
    return (pulse_width * 8191) / 20000;
}

esp_err_t motor_init(uint8_t servo_gpio)
{
    // 1. 定时器配置
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = SERVO_PWM_TIMER,
        .duty_resolution  = LEDC_TIMER_13_BIT, // 13位分辨率
        .freq_hz          = SERVO_PWM_FREQ,    // 50Hz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 2. 通道配置
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = SERVO_PWM_CHANNEL,
        .timer_sel      = SERVO_PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = servo_gpio,
        .duty           = calculate_duty(0), // 初始 0度
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
    
    ESP_LOGI(TAG, "Servo Motor initialized on GPIO %d", servo_gpio);
    return ESP_OK;
}

esp_err_t curtain_control(uint8_t open)
{
    // open = 1 -> 打开窗帘 -> 舵机转到 180度 (假设)
    // open = 0 -> 关闭窗帘 -> 舵机转到 0度
    
    uint32_t duty = calculate_duty(open ? 180 : 0);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL);
    
    // ESP_LOGI(TAG, "Curtain (Servo) set to %s (Angle: %d)", open ? "OPEN" : "CLOSED", open ? 180 : 0);
    return ESP_OK;
}
