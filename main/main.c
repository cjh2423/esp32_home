#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

// 引入模块
#include "config.h"
#include "app_types.h"
#include "app_state.h"
#include "app_control.h"
#include "wifi.h"
#include "http_server.h"

// 引入硬件驱动 (用于初始化)
#include "dht11.h"
#include "bh1750.h"
#include "mq2.h"
#include "led.h"
#include "fan.h"
#include "motor.h"
#include "buzzer.h"
#include "voice_recognition.h"

static const char *TAG = "MAIN";

// WiFi连接回调
static void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "WiFi Connected!");
    char ip_str[16];
    if (wifi_get_ip_string(ip_str, sizeof(ip_str)) == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: %s", ip_str);
    }
}

static void on_wifi_disconnected(void)
{
    ESP_LOGW(TAG, "WiFi Disconnected!");
}

// 模块初始化状态标志
static bool dht11_ok = false;
static bool bh1750_ok = false;
static bool mq2_ok = false;

// 系统主任务
static void system_task(void *pvParameters)
{
    sensor_data_t *sensor_data = app_state_get();
    dht11_data_t dht_data;
    float lux = 0;
    uint32_t smoke_val = 0;
    bool dht_valid = false;
    bool lux_valid = false;
    bool smoke_valid = false;
    
    // 初始化控制逻辑
    app_control_init();

    while (1) {
        dht_valid = false;
        lux_valid = false;
        smoke_valid = false;
        // --- 1. 数据采集 ---
        
        // 读取 DHT11 (仅当初始化成功)
        if (dht11_ok) {
            if (dht11_read(&dht_data) == ESP_OK && dht_data.valid) {
                dht_valid = true;
            } else {
                dht_valid = false;
            }
        }
        
        // 读取 BH1750 (仅当初始化成功)
        if (bh1750_ok) {
            if (bh1750_read(I2C_NUM_0, &lux) == ESP_OK) {
                lux_valid = true;
            } else {
                lux_valid = false;
            }
        }
        
        // 读取 MQ-2 (仅当初始化成功)
        if (mq2_ok) {
            if (mq2_read(MQ2_ADC_CHANNEL, &smoke_val) == ESP_OK) {
                smoke_valid = true;
            } else {
                smoke_valid = false;
            }
        }
        
        // --- 2. 业务逻辑处理 ---
        app_state_lock();
        if (dht_valid) {
            sensor_data->temperature = dht_data.temperature;
            sensor_data->humidity = dht_data.humidity;
        }
        if (lux_valid) {
            sensor_data->light = lux;
        }
        if (smoke_valid) {
            sensor_data->smoke = smoke_val;
        }
        app_control_process(sensor_data);
        app_state_unlock();
        
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL));
    }
}

void app_main(void)
{
    app_state_init();

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Starting Smart Home System...");
    
    // 2. 初始化硬件 (Hardware Layer) - 容错处理
    if (dht11_init(DHT11_GPIO) == ESP_OK) {
        dht11_ok = true;
    } else {
        ESP_LOGE(TAG, "DHT11 Init Failed");
    }
    
    if (bh1750_init(I2C_NUM_0, BH1750_SDA_GPIO, BH1750_SCL_GPIO) == ESP_OK) {
        bh1750_ok = true;
    } else {
        ESP_LOGE(TAG, "BH1750 Init Failed (SDA: %d, SCL: %d)", BH1750_SDA_GPIO, BH1750_SCL_GPIO);
    }
    
    if (mq2_init(MQ2_ADC_CHANNEL) == ESP_OK) {
        mq2_ok = true;
    } else {
        ESP_LOGE(TAG, "MQ-2 Init Failed");
    }
    
    // 执行器初始化（这些通常主要涉及GPIO配置，失败概率极低，且必须成功才能控制）
    // 但为了极致稳健，也可以改为 warn 模式，不过这里保留 ERROR_CHECK 以确保基本功能可用
    // 如果 LED/Fan 引脚配置都失败，通常意味着硬件严重错误或引脚冲突，确实应该 Panic 引起注意
    ESP_ERROR_CHECK(led_init(LED_GPIO, LED_PWM_CHANNEL));
    ESP_ERROR_CHECK(fan_init(FAN_GPIO, FAN_PWM_CHANNEL));
    ESP_ERROR_CHECK(buzzer_init(BUZZER_GPIO));
    
    // 舵机初始化，后两个参数由驱动内部忽略
    ESP_ERROR_CHECK(motor_init(SERVO_GPIO));
    
    // 开机自检
    buzzer_beep(BUZZER_GPIO, 100);
    
    // 3. 初始化网络 (Network Layer)
    sensor_data_t *sensor_data = app_state_get();
    if (wifi_init_sta(WIFI_SSID, WIFI_PASS, on_wifi_connected, on_wifi_disconnected) == ESP_OK) {
        http_server_start(sensor_data);
    } else {
        ESP_LOGE(TAG, "WiFi init failed, HTTP server not started");
    }
    
    // 4. 启动主业务任务 (Application Layer)
    xTaskCreate(system_task, "system_task", 4096, NULL, 5, NULL);
    
    // 5. 初始化语音识别 (Voice Recognition Layer)
    if (vr_init(INMP441_I2S_SCK, INMP441_I2S_WS, INMP441_I2S_SD,
                app_control_handle_voice_command) == ESP_OK) {
        vr_start();
        ESP_LOGI(TAG, "Voice Recognition Started");
    } else {
        ESP_LOGW(TAG, "Voice Recognition Init Failed");
    }
    
    ESP_LOGI(TAG, "System Initialized Successfully");
}
