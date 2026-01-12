#include "app_state.h"

#include <assert.h>
#include <string.h>

static sensor_data_t g_sensor_data;
static SemaphoreHandle_t g_sensor_mutex;

void app_state_init(void)
{
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
    g_sensor_mutex = xSemaphoreCreateMutex();
    assert(g_sensor_mutex != NULL);
}

sensor_data_t *app_state_get(void)
{
    return &g_sensor_data;
}

void app_state_lock(void)
{
    xSemaphoreTake(g_sensor_mutex, portMAX_DELAY);
}

void app_state_unlock(void)
{
    xSemaphoreGive(g_sensor_mutex);
}
