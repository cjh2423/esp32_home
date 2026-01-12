#ifndef APP_STATE_H
#define APP_STATE_H

#include "app_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

// 锁超时时间 (毫秒)
#define APP_STATE_LOCK_TIMEOUT_MS 100

void app_state_init(void);
void app_state_deinit(void);
sensor_data_t *app_state_get(void);

// 带超时的锁操作，返回 ESP_OK 或 ESP_ERR_TIMEOUT
esp_err_t app_state_lock(void);
void app_state_unlock(void);

// 便捷宏：带超时保护的临界区
#define APP_STATE_CRITICAL_SECTION(code) \
    do { \
        if (app_state_lock() == ESP_OK) { \
            code; \
            app_state_unlock(); \
        } \
    } while(0)

#endif // APP_STATE_H
