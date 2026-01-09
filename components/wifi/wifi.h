#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include "esp_event.h"

/**
 * @brief WiFi连接状态回调函数类型
 */
typedef void (*wifi_connected_callback_t)(void);
typedef void (*wifi_disconnected_callback_t)(void);

/**
 * @brief 初始化WiFi并连接到AP
 * 
 * @param ssid WiFi SSID
 * @param password WiFi密码
 * @param connected_cb 连接成功回调
 * @param disconnected_cb 断开连接回调
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t wifi_init_sta(const char *ssid, const char *password,
                        wifi_connected_callback_t connected_cb,
                        wifi_disconnected_callback_t disconnected_cb);

/**
 * @brief 获取WiFi连接状态
 * 
 * @return uint8_t 1-已连接，0-未连接
 */
uint8_t wifi_is_connected(void);

/**
 * @brief 获取IP地址字符串
 * 
 * @param ip_str 输出IP地址字符串缓冲区
 * @param len 缓冲区长度
 * @return esp_err_t ESP_OK 成功，其他值失败
 */
esp_err_t wifi_get_ip_string(char *ip_str, size_t len);

#endif // WIFI_H
