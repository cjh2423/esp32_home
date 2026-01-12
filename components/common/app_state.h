#ifndef APP_STATE_H
#define APP_STATE_H

#include "app_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void app_state_init(void);
sensor_data_t *app_state_get(void);
void app_state_lock(void);
void app_state_unlock(void);

#endif // APP_STATE_H
