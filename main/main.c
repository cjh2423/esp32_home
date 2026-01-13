/**
 * @file main.c
 * @brief ESP32-S3 智能家居系统入口
 *
 *入口文件，所有初始化和任务管理由 application 模块统一处理
 */

#include "application.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = app_start();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Application start failed: %s", esp_err_to_name(ret));
    }
}
