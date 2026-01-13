/**
 * @file application.h
 * @brief 应用层统一入口 - 封装所有初始化和任务管理
 *
 * 设计原则：
 * - main.c 只需调用 app_start() 即可启动整个系统
 * - 所有硬件初始化、任务创建、网络配置均由本模块统一管理
 * - 提供清晰的分层架构：Hardware -> Network -> Application -> Voice
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 任务优先级定义 (数值越大优先级越高)
 *
 * ESP32-S3 FreeRTOS 优先级范围: 0-24 (configMAX_PRIORITIES)
 * 推荐分配:
 * - 0: Idle 任务 (系统保留)
 * - 1-2: 低优先级后台任务
 * - 3-4: 普通应用任务
 * - 5-6: 实时任务 (语音识别)
 * - 7+: 关键系统任务
 */
#define APP_TASK_PRIORITY_LOW       2   // 低优先级 (日志、统计)
#define APP_TASK_PRIORITY_SENSOR    3   // 传感器采集
#define APP_TASK_PRIORITY_CONTROL   4   // 控制逻辑
#define APP_TASK_PRIORITY_VR_DETECT 5   // 语音检测
#define APP_TASK_PRIORITY_VR_FEED   5   // 音频采集 

/**
 * @brief 任务栈大小定义 (单位: 字节)
 *
 * 栈大小估算依据:
 * - 基础开销: ~512 bytes
 * - 局部变量: 根据实际使用
 * - 函数调用深度: 每层 ~100 bytes
 * - 安全余量: 20-30%
 */
#define APP_STACK_SIZE_SENSOR    (2 * 1024)   // 传感器任务 (简单I2C/ADC读取)
#define APP_STACK_SIZE_CONTROL   (2 * 1024)   // 控制任务 (逻辑处理)
#define APP_STACK_SIZE_VR_FEED   (4 * 1024)   // 音频采集 (I2S缓冲)
#define APP_STACK_SIZE_VR_DETECT (8 * 1024)   // 语音识别 (神经网络)

/**
 * @brief 应用配置结构体
 */
typedef struct {
    const char *wifi_ssid;      // WiFi SSID
    const char *wifi_password;  // WiFi 密码
    bool enable_voice;          // 是否启用语音识别
    bool enable_http_server;    // 是否启用 HTTP 服务器
} app_config_t;

/**
 * @brief 默认应用配置
 */
#define APP_CONFIG_DEFAULT() {          \
    .wifi_ssid = NULL,                  \
    .wifi_password = NULL,              \
    .enable_voice = true,               \
    .enable_http_server = true,         \
}

/**
 * @brief 启动应用程序 (使用默认配置)
 *
 * 这是 main.c 唯一需要调用的函数，它将完成：
 * 1. NVS 初始化
 * 2. 硬件驱动初始化 (传感器、执行器)
 * 3. 网络初始化 (WiFi、HTTP)
 * 4. 任务创建 (传感器、控制、语音识别)
 *
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t app_start(void);

/**
 * @brief 启动应用程序 (使用自定义配置)
 *
 * @param config 应用配置
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t app_start_with_config(const app_config_t *config);

/**
 * @brief 获取系统运行状态
 *
 * @return true 系统正常运行
 * @return false 系统未启动或出错
 */
bool app_is_running(void);

/**
 * @brief 获取各模块初始化状态
 */
typedef struct {
    bool nvs_ok;
    bool dht11_ok;
    bool bh1750_ok;
    bool mq2_ok;
    bool led_ok;
    bool fan_ok;
    bool buzzer_ok;
    bool motor_ok;
    bool rgb_led_ok;
    bool wifi_ok;
    bool http_ok;
    bool voice_ok;
} app_init_status_t;

/**
 * @brief 获取初始化状态
 *
 * @return const app_init_status_t* 初始化状态指针
 */
const app_init_status_t* app_get_init_status(void);

#ifdef __cplusplus
}
#endif

#endif // APPLICATION_H
