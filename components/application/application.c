/**
 * @file application.c
 * @brief 应用层统一入口实现
 *
 * 架构设计：
 * ┌─────────────────────────────────────────────────────────────┐
 * │                      Application Layer                       │
 * │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
 * │  │ sensor_task │  │control_task │  │   Voice Recognition  │  │
 * │  │  (Pri: 3)   │  │  (Pri: 4)   │  │ feed(6) + detect(5) │  │
 * │  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
 * │         │                │                     │             │
 * │         └────────┬───────┘                     │             │
 * │                  ▼                             ▼             │
 * │           ┌──────────────┐            ┌───────────────┐      │
 * │           │  app_state   │            │  app_control  │      │
 * │           │ (共享数据)    │◄──────────│  (语音回调)    │      │
 * │           └──────────────┘            └───────────────┘      │
 * └─────────────────────────────────────────────────────────────┘
 * ┌─────────────────────────────────────────────────────────────┐
 * │                      Network Layer                           │
 * │           WiFi Station  +  HTTP Server                       │
 * └─────────────────────────────────────────────────────────────┘
 * ┌─────────────────────────────────────────────────────────────┐
 * │                      Hardware Layer                          │
 * │  DHT11 | BH1750 | MQ2 | LED | FAN | BUZZER | MOTOR | RGB    │
 * └─────────────────────────────────────────────────────────────┘
 */

#include "application.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

// 配置和状态
#include "config.h"
#include "app_types.h"
#include "app_state.h"
#include "app_control.h"

// 网络
#include "wifi.h"
#include "http_server.h"

// 硬件驱动
#include "dht11.h"
#include "bh1750.h"
#include "mq2.h"
#include "led.h"
#include "fan.h"
#include "motor.h"
#include "buzzer.h"
#include "rgb_led.h"
#include "voice_recognition.h"

static const char *TAG = "APP";

// ==================== 模块状态 ====================
static app_init_status_t s_init_status = {0};
static volatile bool s_running = false;

// ==================== 任务句柄 ====================
static TaskHandle_t s_sensor_task_handle = NULL;
static TaskHandle_t s_control_task_handle = NULL;

// ==================== 任务间通信 ====================
static SemaphoreHandle_t s_sensor_data_ready = NULL;

// ==================== 私有函数声明 ====================
static esp_err_t init_nvs(void);
static esp_err_t init_hardware(void);
static esp_err_t init_network(const app_config_t *config);
static esp_err_t init_voice(void);
static esp_err_t start_tasks(void);

static void sensor_task(void *pvParameters);
static void control_task(void *pvParameters);
static void on_wifi_connected(void);
static void on_wifi_disconnected(void);

// ==================== 公共接口实现 ====================

esp_err_t app_start(void)
{
    app_config_t config = {
        .wifi_ssid = WIFI_SSID,
        .wifi_password = WIFI_PASS,
        .enable_voice = true,
        .enable_http_server = true,
    };
    return app_start_with_config(&config);
}

esp_err_t app_start_with_config(const app_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Smart Home System Starting...       ");
    ESP_LOGI(TAG, "========================================");

    // 1. 初始化应用状态
    app_state_init();

    // 2. 创建任务间信号量
    s_sensor_data_ready = xSemaphoreCreateBinary();
    if (s_sensor_data_ready == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_ERR_NO_MEM;
    }

    // 3. 初始化 NVS
    ESP_LOGI(TAG, "[1/5] Initializing NVS...");
    if (init_nvs() != ESP_OK) {
        return ESP_FAIL;
    }

    // 4. 初始化硬件
    ESP_LOGI(TAG, "[2/5] Initializing Hardware...");
    init_hardware();  // 硬件初始化采用容错模式，不返回失败

    // 5. 初始化网络
    ESP_LOGI(TAG, "[3/5] Initializing Network...");
    init_network(config);

    // 6. 启动任务
    ESP_LOGI(TAG, "[4/5] Starting Tasks...");
    if (start_tasks() != ESP_OK) {
        return ESP_FAIL;
    }

    // 7. 初始化语音识别
    if (config->enable_voice) {
        ESP_LOGI(TAG, "[5/5] Initializing Voice Recognition...");
        init_voice();
    }

    // 开机提示
    if (s_init_status.buzzer_ok) {
        buzzer_beep(BUZZER_GPIO, 100);
    }

    s_running = true;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   System Initialized Successfully!    ");
    ESP_LOGI(TAG, "========================================");

    // 打印初始化状态摘要
    ESP_LOGI(TAG, "Module Status:");
    ESP_LOGI(TAG, "  Sensors:  DHT11=%s  BH1750=%s  MQ2=%s",
             s_init_status.dht11_ok ? "OK" : "FAIL",
             s_init_status.bh1750_ok ? "OK" : "FAIL",
             s_init_status.mq2_ok ? "OK" : "FAIL");
    ESP_LOGI(TAG, "  Actuators: LED=%s  FAN=%s  MOTOR=%s  RGB=%s",
             s_init_status.led_ok ? "OK" : "FAIL",
             s_init_status.fan_ok ? "OK" : "FAIL",
             s_init_status.motor_ok ? "OK" : "FAIL",
             s_init_status.rgb_led_ok ? "OK" : "FAIL");
    ESP_LOGI(TAG, "  Network:  WiFi=%s  HTTP=%s",
             s_init_status.wifi_ok ? "OK" : "FAIL",
             s_init_status.http_ok ? "OK" : "FAIL");
    ESP_LOGI(TAG, "  Voice:    %s",
             s_init_status.voice_ok ? "OK" : "DISABLED");

    return ESP_OK;
}

bool app_is_running(void)
{
    return s_running;
}

const app_init_status_t* app_get_init_status(void)
{
    return &s_init_status;
}

// ==================== 私有函数实现 ====================

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    s_init_status.nvs_ok = (ret == ESP_OK);
    return ret;
}

static esp_err_t init_hardware(void)
{
    // ===== 传感器初始化 (容错模式) =====
    if (dht11_init(DHT11_GPIO) == ESP_OK) {
        s_init_status.dht11_ok = true;
        ESP_LOGI(TAG, "  DHT11: OK (GPIO %d)", DHT11_GPIO);
    } else {
        ESP_LOGW(TAG, "  DHT11: FAILED");
    }

    if (bh1750_init(BH1750_SDA_GPIO, BH1750_SCL_GPIO) == ESP_OK) {
        s_init_status.bh1750_ok = true;
        ESP_LOGI(TAG, "  BH1750: OK (SDA=%d, SCL=%d)", BH1750_SDA_GPIO, BH1750_SCL_GPIO);
    } else {
        ESP_LOGW(TAG, "  BH1750: FAILED");
    }

    if (mq2_init(MQ2_ADC_CHANNEL) == ESP_OK) {
        s_init_status.mq2_ok = true;
        ESP_LOGI(TAG, "  MQ2: OK (ADC CH%d)", MQ2_ADC_CHANNEL);
    } else {
        ESP_LOGW(TAG, "  MQ2: FAILED");
    }

    // ===== 执行器初始化 (关键模块) =====
    if (led_init(LED_GPIO, LED_PWM_CHANNEL) == ESP_OK) {
        s_init_status.led_ok = true;
    } else {
        ESP_LOGE(TAG, "  LED: FAILED - Critical!");
    }

    if (fan_init(FAN_GPIO, FAN_PWM_CHANNEL) == ESP_OK) {
        s_init_status.fan_ok = true;
    } else {
        ESP_LOGE(TAG, "  FAN: FAILED - Critical!");
    }

    if (buzzer_init(BUZZER_GPIO) == ESP_OK) {
        s_init_status.buzzer_ok = true;
    } else {
        ESP_LOGW(TAG, "  BUZZER: FAILED");
    }

    if (motor_init(SERVO_GPIO) == ESP_OK) {
        s_init_status.motor_ok = true;
    } else {
        ESP_LOGW(TAG, "  MOTOR: FAILED");
    }

    // ===== RGB LED (可选) =====
    if (rgb_led_init(RGB_LED_GPIO) == ESP_OK) {
        s_init_status.rgb_led_ok = true;
        rgb_led_set_brightness(30);
        rgb_led_blink(RGB_COLOR_GREEN, 2, 100);  // 启动指示
    } else {
        ESP_LOGW(TAG, "  RGB LED: FAILED");
    }

    return ESP_OK;
}

static void on_wifi_connected(void)
{
    char ip_str[16];
    if (wifi_get_ip_string(ip_str, sizeof(ip_str)) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi Connected! IP: %s", ip_str);
    }
}

static void on_wifi_disconnected(void)
{
    ESP_LOGW(TAG, "WiFi Disconnected!");
}

static esp_err_t init_network(const app_config_t *config)
{
    if (config->wifi_ssid == NULL || config->wifi_password == NULL) {
        ESP_LOGW(TAG, "WiFi credentials not provided, skipping network init");
        return ESP_OK;
    }

    if (wifi_init_sta(config->wifi_ssid, config->wifi_password,
                      on_wifi_connected, on_wifi_disconnected) == ESP_OK) {
        s_init_status.wifi_ok = true;

        if (config->enable_http_server) {
            sensor_data_t *sensor_data = app_state_get();
            if (http_server_start(sensor_data) == ESP_OK) {
                s_init_status.http_ok = true;
            }
        }
    } else {
        ESP_LOGE(TAG, "WiFi init failed");
    }

    return ESP_OK;
}

static esp_err_t init_voice(void)
{
    if (vr_init(INMP441_I2S_SCK, INMP441_I2S_WS, INMP441_I2S_SD,
                app_control_handle_voice_command) == ESP_OK) {
        vr_set_vad_callback(app_control_handle_vad_state);
        vr_start();
        s_init_status.voice_ok = true;
        ESP_LOGI(TAG, "Voice Recognition Started");
    } else {
        ESP_LOGW(TAG, "Voice Recognition Init Failed");
    }
    return ESP_OK;
}

static esp_err_t start_tasks(void)
{
    // 初始化控制逻辑
    app_control_init();

    // 创建传感器任务
    BaseType_t ret = xTaskCreate(
        sensor_task,
        "sensor_task",
        APP_STACK_SIZE_SENSOR,
        NULL,
        APP_TASK_PRIORITY_SENSOR,
        &s_sensor_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }

    // 创建控制任务
    ret = xTaskCreate(
        control_task,
        "control_task",
        APP_STACK_SIZE_CONTROL,
        NULL,
        APP_TASK_PRIORITY_CONTROL,
        &s_control_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create control task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Tasks created:");
    ESP_LOGI(TAG, "  sensor_task:  Pri=%d, Stack=%d bytes",
             APP_TASK_PRIORITY_SENSOR, APP_STACK_SIZE_SENSOR);
    ESP_LOGI(TAG, "  control_task: Pri=%d, Stack=%d bytes",
             APP_TASK_PRIORITY_CONTROL, APP_STACK_SIZE_CONTROL);

    return ESP_OK;
}

// ==================== 任务实现 ====================

/**
 * @brief 传感器采集任务
 *
 * 职责：定期读取所有传感器数据并更新到共享状态
 * 特点：
 * - 独立的传感器读取任务，不阻塞控制逻辑
 * - 读取完成后通过信号量通知控制任务
 * - 容错处理：传感器读取失败不影响其他传感器
 */
static void sensor_task(void *pvParameters)
{
    sensor_data_t *sensor_data = app_state_get();
    dht11_data_t dht_data;
    float lux = 0;
    uint32_t smoke_val = 0;

    ESP_LOGI(TAG, "Sensor task started");

    while (1) {
        bool any_update = false;

        // 读取 DHT11
        if (s_init_status.dht11_ok) {
            if (dht11_read(&dht_data) == ESP_OK && dht_data.valid) {
                if (app_state_lock() == ESP_OK) {
                    sensor_data->temperature = dht_data.temperature;
                    sensor_data->humidity = dht_data.humidity;
                    app_state_unlock();
                    any_update = true;
                }
            }
        }

        // 读取 BH1750
        if (s_init_status.bh1750_ok) {
            if (bh1750_read(&lux) == ESP_OK) {
                if (app_state_lock() == ESP_OK) {
                    sensor_data->light = lux;
                    app_state_unlock();
                    any_update = true;
                }
            }
        }

        // 读取 MQ2
        if (s_init_status.mq2_ok) {
            if (mq2_read(MQ2_ADC_CHANNEL, &smoke_val) == ESP_OK) {
                if (app_state_lock() == ESP_OK) {
                    sensor_data->smoke = smoke_val;
                    app_state_unlock();
                    any_update = true;
                }
            }
        }

        // 通知控制任务有新数据
        if (any_update) {
            xSemaphoreGive(s_sensor_data_ready);
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL));
    }
}

/**
 * @brief 控制逻辑任务
 *
 * 职责：根据传感器数据执行自动化控制逻辑
 * 特点：
 * - 等待传感器数据更新信号
 * - 优先级高于传感器任务，确保及时响应
 * - 处理烟雾报警等紧急情况
 */
static void control_task(void *pvParameters)
{
    sensor_data_t *sensor_data = app_state_get();

    ESP_LOGI(TAG, "Control task started");

    while (1) {
        // 等待传感器数据更新 (最多等待 2 倍采样周期)
        if (xSemaphoreTake(s_sensor_data_ready, pdMS_TO_TICKS(SENSOR_READ_INTERVAL * 2)) == pdTRUE) {
            // 执行控制逻辑
            if (app_state_lock() == ESP_OK) {
                app_control_process(sensor_data);
                app_state_unlock();
            }
        }
        // 即使没有新数据，也定期执行一次控制逻辑（确保系统响应）
        else {
            if (app_state_lock() == ESP_OK) {
                app_control_process(sensor_data);
                app_state_unlock();
            }
        }
    }
}
