#ifndef APP_CONTROL_H
#define APP_CONTROL_H

#include "app_types.h"
#include "esp_err.h"

/**
 * @brief 初始化应用控制模块
 * 
 * @return esp_err_t 
 */
esp_err_t app_control_init(void);

/**
 * @brief 执行一次自动化控制逻辑
 * 根据传入的传感器数据，更新设备状态并控制硬件
 * 
 * @param data 指向全局传感器数据的指针
 */
void app_control_process(sensor_data_t *data);

#endif // APP_CONTROL_H
