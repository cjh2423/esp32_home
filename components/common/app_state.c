#include "app_state.h"

#include "esp_assert.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "APP_STATE";
static sensor_data_t g_sensor_data;
static SemaphoreHandle_t g_sensor_mutex = NULL;

void app_state_init(void)
{
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
    g_sensor_mutex = xSemaphoreCreateMutex();
    assert(g_sensor_mutex != NULL);
}

void app_state_deinit(void)
{
    if (g_sensor_mutex != NULL) {
        vSemaphoreDelete(g_sensor_mutex);
        g_sensor_mutex = NULL;
    }
}

sensor_data_t *app_state_get(void)
{
    return &g_sensor_data;
}

esp_err_t app_state_lock(void)
{
    if (g_sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_sensor_mutex, pdMS_TO_TICKS(APP_STATE_LOCK_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Lock timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void app_state_unlock(void)
{
    if (g_sensor_mutex != NULL) {
        xSemaphoreGive(g_sensor_mutex);
    }
}
