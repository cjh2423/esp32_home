#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "app_types.h" // 引用公共类型

// 移除原有的 sensor_data_t 定义，直接使用 app_types.h 中的定义

/**
 * @brief 启动HTTP服务器
 * 
 * @param sensor_data 传感器数据指针（用于共享数据）
 * @return httpd_handle_t 服务器句柄，NULL表示失败
 */
httpd_handle_t http_server_start(sensor_data_t *sensor_data);

/**
 * @brief 停止HTTP服务器
 * 
 * @param server 服务器句柄
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t http_server_stop(httpd_handle_t server);

#endif // HTTP_SERVER_H
